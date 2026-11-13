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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal.h"
#include <stdio.h>
#include <string.h>

/*Rewrite support: replace the OpusTags (comment) header of the first link of
   an Ogg Opus file, leaving all audio data untouched.
  The new header may occupy a different number of pages than the old one, so
   the file is rewritten to a temporary file: fresh header pages are emitted
   with libogg, and the remaining pages of the first link are copied with
   their page sequence numbers renumbered (and checksums recomputed) to keep
   the stream contiguous.
  Pages belonging to subsequent links of a chained stream (different serial
   numbers) are copied verbatim.*/

/*Read a single Ogg page from _f.
  The header and body are stored in newly allocated buffers which the caller
   must free with _ogg_free().
  Returns 1 on success, 0 on a clean end-of-file (no bytes read), or a
   negative value on error.*/
static int op_read_page(FILE *_f,ogg_page *_og,
 unsigned char **_hbuf,unsigned char **_bbuf){
  unsigned char  header[27];
  unsigned char *hbuf;
  unsigned char *bbuf;
  size_t         body_sz;
  size_t         nread;
  int            nsegments;
  int            si;
  *_hbuf=NULL;
  *_bbuf=NULL;
  nread=fread(header,1,27,_f);
  if(nread==0)return 0;
  if(nread<27)return OP_EBADHEADER;
  if(memcmp(header,"OggS",4)!=0)return OP_EBADHEADER;
  if(header[4]!=0)return OP_EVERSION;
  nsegments=header[26];
  hbuf=(unsigned char *)_ogg_malloc(sizeof(*hbuf)*(27+nsegments));
  if(OP_UNLIKELY(hbuf==NULL))return OP_EFAULT;
  memcpy(hbuf,header,27);
  nread=fread(hbuf+27,1,(size_t)nsegments,_f);
  if(nread<(size_t)nsegments){
    _ogg_free(hbuf);
    return OP_EBADHEADER;
  }
  body_sz=0;
  for(si=0;si<nsegments;si++)body_sz+=hbuf[27+si];
  bbuf=(unsigned char *)_ogg_malloc(sizeof(*bbuf)*(body_sz+1));
  if(OP_UNLIKELY(bbuf==NULL)){
    _ogg_free(hbuf);
    return OP_EFAULT;
  }
  nread=fread(bbuf,1,body_sz,_f);
  if(nread<body_sz){
    _ogg_free(bbuf);
    _ogg_free(hbuf);
    return OP_EBADHEADER;
  }
  _og->header=hbuf;
  _og->header_len=27+nsegments;
  _og->body=bbuf;
  _og->body_len=(long)body_sz;
  *_hbuf=hbuf;
  *_bbuf=bbuf;
  return 1;
}

/*Write a single Ogg page to _f.
  Returns 0 on success or #OP_EREAD on error.*/
static int op_write_page(FILE *_f,const ogg_page *_og){
  if(fwrite(_og->header,1,(size_t)_og->header_len,_f)
   !=(size_t)_og->header_len){
    return OP_EREAD;
  }
  if(fwrite(_og->body,1,(size_t)_og->body_len,_f)!=(size_t)_og->body_len){
    return OP_EREAD;
  }
  return 0;
}

int op_write_tags(const char *_path,const OpusTags *_tags){
  ogg_stream_state  os_in;
  ogg_stream_state  os_out;
  unsigned char    *tags_packet;
  unsigned char    *head_packet;
  char             *tmp_path;
  FILE             *in;
  FILE             *out;
  size_t            tags_packet_len;
  size_t            head_packet_len;
  long              serialno;
  long              data_offset;
  long              pageno;
  int               os_in_ready;
  int               os_out_ready;
  int               ret;
  /*Serialize the new tags first, so invalid fields are rejected before the
     file is touched.*/
  tags_packet=NULL;
  ret=opus_tags_to_packet(_tags,&tags_packet,&tags_packet_len);
  if(ret<0)return ret;
  head_packet=NULL;
  tmp_path=NULL;
  in=NULL;
  out=NULL;
  os_in_ready=0;
  os_out_ready=0;
  head_packet_len=0;
  serialno=0;
  data_offset=0;
  pageno=0;
  /*Open the file read-write: this also fails for read-only files, ensuring
     read-only paths are left untouched.*/
  in=fopen(_path,"r+b");
  if(in==NULL){
    ret=OP_EREAD;
    goto fail;
  }
  /*Read the first page and extract the OpusHead packet.*/
  {
    ogg_page       og;
    unsigned char *hbuf;
    unsigned char *bbuf;
    ret=op_read_page(in,&og,&hbuf,&bbuf);
    if(ret==0)ret=OP_ENOTFORMAT;
    if(ret<0)goto fail;
    if(!ogg_page_bos(&og)){
      _ogg_free(hbuf);
      _ogg_free(bbuf);
      ret=OP_ENOTFORMAT;
      goto fail;
    }
    serialno=ogg_page_serialno(&og);
    ogg_stream_init(&os_in,(int)serialno);
    os_in_ready=1;
    if(ogg_stream_pagein(&os_in,&og)!=0){
      _ogg_free(hbuf);
      _ogg_free(bbuf);
      ret=OP_EBADHEADER;
      goto fail;
    }
    _ogg_free(hbuf);
    _ogg_free(bbuf);
  }
  {
    ogg_packet pkt;
    if(ogg_stream_packetout(&os_in,&pkt)!=1||!pkt.b_o_s){
      ret=OP_ENOTFORMAT;
      goto fail;
    }
    ret=opus_head_parse(NULL,pkt.packet,(size_t)pkt.bytes);
    if(ret<0)goto fail;
    /*Keep our own copy of the OpusHead packet for the new header.*/
    head_packet=(unsigned char *)_ogg_malloc(sizeof(*head_packet)*pkt.bytes);
    if(OP_UNLIKELY(head_packet==NULL)){
      ret=OP_EFAULT;
      goto fail;
    }
    memcpy(head_packet,pkt.packet,(size_t)pkt.bytes);
    head_packet_len=(size_t)pkt.bytes;
  }
  /*Read pages until the OpusTags packet (the second header packet)
     completes.*/
  {
    int got_tags;
    got_tags=0;
    while(!got_tags){
      ogg_page       og;
      unsigned char *hbuf;
      unsigned char *bbuf;
      ret=op_read_page(in,&og,&hbuf,&bbuf);
      if(ret==0)ret=OP_EBADHEADER;
      if(ret<0)goto fail;
      if(ogg_page_serialno(&og)!=serialno
       ||ogg_stream_pagein(&os_in,&og)!=0){
        _ogg_free(hbuf);
        _ogg_free(bbuf);
        ret=OP_EBADHEADER;
        goto fail;
      }
      _ogg_free(hbuf);
      _ogg_free(bbuf);
      for(;;){
        ogg_packet pkt;
        int        pkt_ret;
        pkt_ret=ogg_stream_packetout(&os_in,&pkt);
        if(pkt_ret<0){
          ret=OP_EBADHEADER;
          goto fail;
        }
        if(pkt_ret==0)break;
        if(got_tags){
          /*The OpusTags packet must be the last thing on the header pages:
             the first audio packet must begin on a fresh page.*/
          ret=OP_EBADHEADER;
          goto fail;
        }
        ret=opus_tags_parse(NULL,pkt.packet,(size_t)pkt.bytes);
        if(ret<0)goto fail;
        got_tags=1;
      }
    }
  }
  /*The rest of the file starts on a page boundary here.*/
  data_offset=ftell(in);
  if(data_offset<0){
    ret=OP_EREAD;
    goto fail;
  }
  /*Emit the new header pages.*/
  ogg_stream_init(&os_out,(int)serialno);
  os_out_ready=1;
  {
    ogg_packet pkt;
    memset(&pkt,0,sizeof(pkt));
    pkt.packet=head_packet;
    pkt.bytes=(long)head_packet_len;
    pkt.b_o_s=1;
    pkt.packetno=0;
    if(ogg_stream_packetin(&os_out,&pkt)!=0){
      ret=OP_EFAULT;
      goto fail;
    }
    memset(&pkt,0,sizeof(pkt));
    pkt.packet=tags_packet;
    pkt.bytes=(long)tags_packet_len;
    pkt.b_o_s=0;
    pkt.packetno=1;
    if(ogg_stream_packetin(&os_out,&pkt)!=0){
      ret=OP_EFAULT;
      goto fail;
    }
  }
  /*+14: room for the ".opustags.tmp" suffix and the terminating NUL.*/
  tmp_path=(char *)_ogg_malloc(sizeof(*tmp_path)*(strlen(_path)+14));
  if(OP_UNLIKELY(tmp_path==NULL)){
    ret=OP_EFAULT;
    goto fail;
  }
  strcpy(tmp_path,_path);
  strcat(tmp_path,".opustags.tmp");
  out=fopen(tmp_path,"wb");
  if(out==NULL){
    ret=OP_EREAD;
    goto fail;
  }
  /*Flush the header pages (OpusHead and OpusTags) to the temporary file.*/
  {
    ogg_page og;
    while(ogg_stream_flush(&os_out,&og)!=0){
      ret=op_write_page(out,&og);
      if(ret<0)goto fail;
      pageno++;
    }
  }
  /*Copy the remaining pages, renumbering the pages of the first link and
     recomputing their checksums; pages of other links are copied verbatim.*/
  if(fseek(in,data_offset,SEEK_SET)!=0){
    ret=OP_EREAD;
    goto fail;
  }
  for(;;){
    ogg_page       og;
    unsigned char *hbuf;
    unsigned char *bbuf;
    ret=op_read_page(in,&og,&hbuf,&bbuf);
    if(ret<0)goto fail;
    if(ret==0)break;
    if(ogg_page_serialno(&og)==serialno){
      /*Patch the page sequence number and fix up the checksum.*/
      hbuf[18]=(unsigned char)(pageno&0xFF);
      hbuf[19]=(unsigned char)(pageno>>8&0xFF);
      hbuf[20]=(unsigned char)(pageno>>16&0xFF);
      hbuf[21]=(unsigned char)(pageno>>24&0xFF);
      ogg_page_checksum_set(&og);
      pageno++;
    }
    ret=op_write_page(out,&og);
    _ogg_free(hbuf);
    _ogg_free(bbuf);
    if(ret<0)goto fail;
  }
  if(fclose(in)!=0){
    in=NULL;
    ret=OP_EREAD;
    goto fail;
  }
  in=NULL;
  if(fclose(out)!=0){
    out=NULL;
    ret=OP_EREAD;
    goto fail;
  }
  out=NULL;
  /*Atomically replace the original file.*/
  if(rename(tmp_path,_path)!=0){
    ret=OP_EREAD;
    goto fail;
  }
  ret=0;
fail:
  if(in!=NULL)fclose(in);
  if(out!=NULL)fclose(out);
  if(ret<0&&tmp_path!=NULL)remove(tmp_path);
  if(os_out_ready)ogg_stream_clear(&os_out);
  if(os_in_ready)ogg_stream_clear(&os_in);
  _ogg_free(tmp_path);
  _ogg_free(head_packet);
  _ogg_free(tags_packet);
  return ret;
}
