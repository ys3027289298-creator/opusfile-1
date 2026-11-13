/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE libopusfile SOFTWARE CODEC SOURCE CODE. *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE libopusfile SOURCE CODE IS (C) COPYRIGHT 2012-2020           *
 * by the Xiph.Org Foundation and contributors https://xiph.org/    *
 *                                                                  *
 ********************************************************************/
/*Example program to list and edit the tags and cover art of a .opus file.

  Usage:
    tagging_example <file.opus>
        Print the vendor string, all tags, and a summary of each embedded
         picture.
    tagging_example <file.opus> [--dump-picture=<out>]
        Also write the data of the first embedded picture to <out>.
    tagging_example <file.opus> [options] [TAG=VALUE ...]
        Replace the listed tags (any existing tags with the same name are
         removed first), then write the tags back to the file and print the
         result read back from the file.

  Options for the editing form:
    --picture=<image>     Embed <image> (PNG, JPEG, or GIF) as cover art,
                           replacing any existing pictures.
    --picture-type=<n>    Use picture type <n> (0-20, default 3: front cover)
                           for --picture.
    --clear-pictures      Remove all embedded pictures.

  The file is rewritten in place; the audio data is not modified.*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <opusfile.h>
#if defined(_WIN32)
# include "win32utf8.h"
#endif

static void usage(const char *_argv0){
  fprintf(stderr,"Usage: %s <file.opus> [--dump-picture=<out>]\n",_argv0);
  fprintf(stderr,"       %s <file.opus> [--picture=<image>] "
   "[--picture-type=<n>] [--clear-pictures] [TAG=VALUE ...]\n",_argv0);
}

/*Print the tags, decoding any METADATA_BLOCK_PICTURE entries.
  If _dump_path is not NULL, the data of the first picture is written to that
   path.*/
static int print_tags(const OpusTags *_tags,const char *_dump_path){
  int ci;
  int dumped;
  fprintf(stderr,"Vendor: %s\n",
   _tags->vendor==NULL?"(none)":_tags->vendor);
  fprintf(stderr,"Comments: %i\n",_tags->comments);
  dumped=0;
  for(ci=0;ci<_tags->comments;ci++){
    const char *comment;
    comment=_tags->user_comments[ci];
    if(opus_tagncompare("METADATA_BLOCK_PICTURE",22,comment)==0){
      OpusPictureTag pic;
      int            ret;
      ret=opus_picture_tag_parse(&pic,comment);
      if(ret<0){
        fprintf(stderr,"  METADATA_BLOCK_PICTURE: unparseable (%i)\n",ret);
        continue;
      }
      fprintf(stderr,"  METADATA_BLOCK_PICTURE: type=%li mime=\"%s\" "
       "description=\"%s\" %lux%lux%lu colors=%lu data=%lu bytes",
       (long)pic.type,pic.mime_type,pic.description,(unsigned long)pic.width,
       (unsigned long)pic.height,(unsigned long)pic.depth,
       (unsigned long)pic.colors,(unsigned long)pic.data_length);
      switch(pic.format){
        case OP_PIC_FORMAT_URL: fprintf(stderr," [URL]");break;
        case OP_PIC_FORMAT_JPEG:fprintf(stderr," [JPEG]");break;
        case OP_PIC_FORMAT_PNG: fprintf(stderr," [PNG]");break;
        case OP_PIC_FORMAT_GIF: fprintf(stderr," [GIF]");break;
        default:                fprintf(stderr," [unknown format]");break;
      }
      fprintf(stderr,"\n");
      if(_dump_path!=NULL&&!dumped){
        FILE *out;
        out=fopen(_dump_path,"wb");
        if(out==NULL||fwrite(pic.data,1,pic.data_length,out)
         !=pic.data_length){
          fprintf(stderr,"Error writing picture data to '%s'.\n",_dump_path);
          opus_picture_tag_clear(&pic);
          return EXIT_FAILURE;
        }
        if(out!=NULL)fclose(out);
        dumped=1;
      }
      opus_picture_tag_clear(&pic);
    }
    else fprintf(stderr,"  %s\n",comment);
  }
  return 0;
}

/*Open _path and print its tags.
  Returns 0 on success or a negative libopusfile error code.*/
static int print_file_tags(const char *_path,const char *_dump_path){
  OggOpusFile *of;
  int          ret;
  of=op_open_file(_path,&ret);
  if(of==NULL)return ret;
  ret=print_tags(op_tags(of,0),_dump_path);
  op_free(of);
  return ret;
}

/*Remove all comments named _tag_name (case-insensitive) from _tags.*/
static void remove_comments(OpusTags *_tags,const char *_tag_name,
 int _tag_name_len){
  int ci;
  ci=0;
  while(ci<_tags->comments){
    if(opus_tagncompare(_tag_name,_tag_name_len,
     _tags->user_comments[ci])==0){
      int cj;
      _ogg_free(_tags->user_comments[ci]);
      /*Shift the remaining comments (and the binary suffix slot at index
         comments) down by one.*/
      for(cj=ci+1;cj<=_tags->comments;cj++){
        _tags->user_comments[cj-1]=_tags->user_comments[cj];
        _tags->comment_lengths[cj-1]=_tags->comment_lengths[cj];
      }
      _tags->comments--;
    }
    else ci++;
  }
}

/*Sniff the MIME type of an image file from its magic bytes.*/
static const char *sniff_mime_type(const unsigned char *_data,size_t _len){
  static const unsigned char PNG_SIG[8]={0x89,0x50,0x4E,0x47,0x0D,0x0A,
   0x1A,0x0A};
  if(_len>=8&&memcmp(_data,PNG_SIG,8)==0)return "image/png";
  if(_len>=3&&_data[0]==0xFF&&_data[1]==0xD8&&_data[2]==0xFF){
    return "image/jpeg";
  }
  if(_len>=6&&memcmp(_data,"GIF8",4)==0)return "image/gif";
  return NULL;
}

/*Read an entire file into a newly allocated buffer.*/
static unsigned char *read_file(const char *_path,size_t *_len){
  unsigned char *data;
  FILE          *in;
  long           size;
  in=fopen(_path,"rb");
  if(in==NULL)return NULL;
  if(fseek(in,0,SEEK_END)!=0||(size=ftell(in))<0
   ||fseek(in,0,SEEK_SET)!=0){
    fclose(in);
    return NULL;
  }
  data=(unsigned char *)malloc(sizeof(*data)*(size_t)size);
  if(data==NULL
   ||size>0&&fread(data,1,(size_t)size,in)!=(size_t)size){
    free(data);
    fclose(in);
    return NULL;
  }
  fclose(in);
  *_len=(size_t)size;
  return data;
}

int main(int _argc,char **_argv){
  const char *path;
  const char *picture_path;
  const char *dump_path;
  long        picture_type;
  int         clear_pictures;
  int         first_tag;
  int         ai;
  int         ret;
#if defined(_WIN32)
  win32_utf8_setup(&_argc,&_argv);
#endif
  if(_argc<2){
    usage(_argv[0]);
    return EXIT_FAILURE;
  }
  path=_argv[1];
  picture_path=NULL;
  dump_path=NULL;
  picture_type=3;
  clear_pictures=0;
  first_tag=_argc;
  for(ai=2;ai<_argc;ai++){
    const char *arg;
    arg=_argv[ai];
    if(arg[0]!='-'||arg[1]!='-'){
      first_tag=ai;
      break;
    }
    if(strncmp(arg,"--picture-type=",15)==0){
      char *end;
      picture_type=strtol(arg+15,&end,10);
      if(*end!='\0'){
        fprintf(stderr,"Invalid picture type: '%s'\n",arg+15);
        return EXIT_FAILURE;
      }
    }
    else if(strncmp(arg,"--picture=",10)==0)picture_path=arg+10;
    else if(strncmp(arg,"--dump-picture=",15)==0)dump_path=arg+15;
    else if(strcmp(arg,"--clear-pictures")==0)clear_pictures=1;
    else{
      fprintf(stderr,"Unknown option: '%s'\n",arg);
      usage(_argv[0]);
      return EXIT_FAILURE;
    }
  }
  /*Options may also appear after the tag arguments.*/
  for(ai=first_tag;ai<_argc;ai++){
    const char *arg;
    arg=_argv[ai];
    if(arg[0]!='-'||arg[1]!='-')continue;
    if(strncmp(arg,"--picture-type=",15)==0){
      char *end;
      picture_type=strtol(arg+15,&end,10);
      if(*end!='\0'){
        fprintf(stderr,"Invalid picture type: '%s'\n",arg+15);
        return EXIT_FAILURE;
      }
    }
    else if(strncmp(arg,"--picture=",10)==0)picture_path=arg+10;
    else if(strncmp(arg,"--dump-picture=",15)==0)dump_path=arg+15;
    else if(strcmp(arg,"--clear-pictures")==0)clear_pictures=1;
    else{
      fprintf(stderr,"Unknown option: '%s'\n",arg);
      usage(_argv[0]);
      return EXIT_FAILURE;
    }
  }
  if(first_tag>=_argc&&picture_path==NULL&&!clear_pictures){
    /*Read-only mode: just print the tags.*/
    ret=print_file_tags(path,dump_path);
    if(ret<0)fprintf(stderr,"Error opening '%s': %i\n",path,ret);
    return ret<0?EXIT_FAILURE:0;
  }
  /*Editing mode: read the current tags, modify them, and write them back.*/
  {
    OggOpusFile *of;
    OpusTags     tags;
    of=op_open_file(path,&ret);
    if(of==NULL){
      fprintf(stderr,"Error opening '%s': %i\n",path,ret);
      return EXIT_FAILURE;
    }
    opus_tags_init(&tags);
    ret=opus_tags_copy(&tags,op_tags(of,0));
    op_free(of);
    if(ret<0){
      fprintf(stderr,"Error copying tags: %i\n",ret);
      opus_tags_clear(&tags);
      return EXIT_FAILURE;
    }
    /*Apply the TAG=VALUE arguments.*/
    for(ai=2;ai<_argc;ai++){
      const char *arg;
      const char *eq;
      char       *value;
      arg=_argv[ai];
      if(arg[0]=='-'&&arg[1]=='-')continue;
      eq=strchr(arg,'=');
      if(eq==NULL){
        /*Leave the malformed comment in place: the library will reject it
           when the tags are serialized for writing.*/
        ret=opus_tags_add_comment(&tags,arg);
      }
      else{
        char *tag_name;
        tag_name=(char *)malloc((size_t)(eq-arg)+1);
        value=(char *)malloc(strlen(eq+1)+1);
        if(tag_name==NULL||value==NULL){
          free(tag_name);
          free(value);
          opus_tags_clear(&tags);
          return EXIT_FAILURE;
        }
        memcpy(tag_name,arg,(size_t)(eq-arg));
        tag_name[eq-arg]='\0';
        strcpy(value,eq+1);
        remove_comments(&tags,arg,(int)(eq-arg));
        ret=opus_tags_add(&tags,tag_name,value);
        free(tag_name);
        free(value);
      }
      if(ret<0){
        fprintf(stderr,"Error adding tag '%s': %i\n",arg,ret);
        opus_tags_clear(&tags);
        return EXIT_FAILURE;
      }
    }
    /*Replace the pictures if requested.*/
    if(clear_pictures||picture_path!=NULL){
      remove_comments(&tags,"METADATA_BLOCK_PICTURE",22);
    }
    if(picture_path!=NULL){
      unsigned char  *data;
      OpusPictureTag pic;
      const char     *mime_type;
      char           *picture_tag;
      size_t          data_length;
      data=read_file(picture_path,&data_length);
      if(data==NULL){
        fprintf(stderr,"Error reading picture file '%s'.\n",picture_path);
        opus_tags_clear(&tags);
        return EXIT_FAILURE;
      }
      mime_type=sniff_mime_type(data,data_length);
      if(mime_type==NULL){
        fprintf(stderr,"Unrecognized image format in '%s'.\n",picture_path);
        free(data);
        opus_tags_clear(&tags);
        return EXIT_FAILURE;
      }
      opus_picture_tag_init(&pic);
      pic.type=(opus_int32)picture_type;
      pic.mime_type=(char *)mime_type;
      pic.description=(char *)"";
      pic.data_length=(opus_uint32)data_length;
      pic.data=data;
      ret=opus_picture_tag_to_string(&pic,&picture_tag);
      free(data);
      if(ret<0){
        fprintf(stderr,"Error serializing picture: %i\n",ret);
        opus_tags_clear(&tags);
        return EXIT_FAILURE;
      }
      ret=opus_tags_add_comment(&tags,picture_tag);
      _ogg_free(picture_tag);
      if(ret<0){
        fprintf(stderr,"Error adding picture tag: %i\n",ret);
        opus_tags_clear(&tags);
        return EXIT_FAILURE;
      }
    }
    /*Write the new tags back to the file.*/
    ret=op_write_tags(path,&tags);
    opus_tags_clear(&tags);
    if(ret<0){
      fprintf(stderr,"Error writing tags to '%s': %i\n",path,ret);
      return EXIT_FAILURE;
    }
    fprintf(stderr,"Tags written. Read back from file:\n");
    ret=print_file_tags(path,dump_path);
    if(ret<0){
      fprintf(stderr,"Error re-opening '%s': %i\n",path,ret);
      return EXIT_FAILURE;
    }
  }
  return 0;
}
