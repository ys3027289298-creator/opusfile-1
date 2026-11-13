/*Verification for the tag/cover serialization round-trip and the rejection
   of overlong or illegal fields.
  Build against the in-tree static library, e.g.:
    cc -Iinclude -I<opus headers> output/tag_limits_test.c \
       unix/libopusfile.a -logg -lopus -o output/tag_limits_test
  Prints one line per check and exits non-zero if any check fails.*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <opusfile.h>

static int nfail;
static int npass;

static void check(int _ok,const char *_what){
  if(_ok){
    npass++;
    printf("PASS: %s\n",_what);
  }
  else{
    nfail++;
    printf("FAIL: %s\n",_what);
  }
}

/*Round-trip an OpusTags structure through opus_tags_to_packet() and
   opus_tags_parse() and verify everything (including the binary suffix)
   survives.*/
static void test_tags_roundtrip(void){
  static const unsigned char SUFFIX[3]={0x03,0xAA,0xBB};
  OpusTags      tags;
  OpusTags      back;
  unsigned char *packet;
  const unsigned char *suffix;
  size_t         packet_len;
  int            suffix_len;
  int            ret;
  int            ci;
  int            same;
  opus_tags_init(&tags);
  free(tags.vendor);
  tags.vendor=(char *)malloc(8);
  strcpy(tags.vendor,"test/1.0");
  opus_tags_add(&tags,"TITLE","Round Trip");
  opus_tags_add(&tags,"ARTIST","\xE5\xB0\x81\xE9\x9D\xA2");
  opus_tags_add_comment(&tags,"COMMENT=with \xC3\xA9 utf-8");
  opus_tags_set_binary_suffix(&tags,SUFFIX,3);
  packet=NULL;
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==0&&packet!=NULL,"tags serialize");
  ret=opus_tags_parse(&back,packet,packet_len);
  check(ret==0,"tags re-parse");
  same=back.comments==tags.comments
   &&back.vendor!=NULL&&strcmp(back.vendor,tags.vendor)==0;
  for(ci=0;same&&ci<tags.comments;ci++){
    same=back.comment_lengths[ci]==tags.comment_lengths[ci]
     &&memcmp(back.user_comments[ci],tags.user_comments[ci],
     (size_t)tags.comment_lengths[ci])==0;
  }
  check(same,"tags round-trip identical");
  suffix=opus_tags_get_binary_suffix(&back,&suffix_len);
  check(suffix_len==3&&suffix!=NULL&&memcmp(suffix,SUFFIX,3)==0,
   "binary suffix round-trip");
  opus_tags_clear(&back);
  opus_tags_clear(&tags);
  _ogg_free(packet);
}

/*Illegal or overlong tag fields must be rejected with OP_EINVAL.*/
static void test_tags_reject(void){
  OpusTags      tags;
  unsigned char *packet;
  size_t         packet_len;
  int            ret;
  /*Comment without '='.*/
  opus_tags_init(&tags);
  opus_tags_add_comment(&tags,"NOEQUALS");
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==OP_EINVAL,"reject comment without '='");
  opus_tags_clear(&tags);
  /*Empty tag name.*/
  opus_tags_init(&tags);
  opus_tags_add_comment(&tags,"=value");
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==OP_EINVAL,"reject empty tag name");
  opus_tags_clear(&tags);
  /*Control character in tag name.*/
  opus_tags_init(&tags);
  opus_tags_add_comment(&tags,"BAD\x01""TAG=value");
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==OP_EINVAL,"reject control char in tag name");
  opus_tags_clear(&tags);
  /*DEL character in tag name.*/
  opus_tags_init(&tags);
  opus_tags_add_comment(&tags,"BAD\x7F""TAG=value");
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==OP_EINVAL,"reject 0x7F in tag name");
  opus_tags_clear(&tags);
  /*Negative comment length (field too long/corrupt).*/
  opus_tags_init(&tags);
  opus_tags_add_comment(&tags,"TITLE=x");
  tags.comment_lengths[0]=-1;
  ret=opus_tags_to_packet(&tags,&packet,&packet_len);
  check(ret==OP_EINVAL,"reject negative comment length");
  opus_tags_clear(&tags);
}

static void fill_pic(OpusPictureTag *_pic,unsigned char *_data,size_t _len){
  size_t i;
  opus_picture_tag_init(_pic);
  _pic->type=3;
  _pic->mime_type=(char *)"image/png";
  _pic->description=(char *)"\xE5\xB0\x81\xE9\x9D\xA2";
  _pic->width=32;
  _pic->height=32;
  _pic->depth=24;
  _pic->colors=0;
  for(i=0;i<_len;i++)_data[i]=(unsigned char)(i*31+7);
  _pic->data_length=(opus_uint32)_len;
  _pic->data=_data;
}

/*Round-trip a picture tag and verify the type, MIME string, and every data
   byte survive.*/
static void test_picture_roundtrip(void){
  unsigned char   data[257];
  OpusPictureTag  pic;
  OpusPictureTag  back;
  char           *tag;
  int             ret;
  int             same;
  fill_pic(&pic,data,sizeof(data));
  tag=NULL;
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==0&&tag!=NULL
   &&strncmp(tag,"METADATA_BLOCK_PICTURE=",23)==0,"picture serialize");
  ret=opus_picture_tag_parse(&back,tag);
  check(ret==0,"picture re-parse");
  same=back.type==pic.type
   &&strcmp(back.mime_type,pic.mime_type)==0
   &&strcmp(back.description,pic.description)==0
   &&back.width==pic.width&&back.height==pic.height
   &&back.depth==pic.depth&&back.colors==pic.colors
   &&back.data_length==pic.data_length
   &&memcmp(back.data,pic.data,pic.data_length)==0;
  check(same,"picture round-trip identical (bytes+type+mime)");
  opus_picture_tag_clear(&back);
  _ogg_free(tag);
}

/*Illegal picture fields must be rejected with OP_EINVAL.*/
static void test_picture_reject(void){
  unsigned char  data[4];
  OpusPictureTag pic;
  char          *tag;
  int            ret;
  /*Picture type out of range.*/
  fill_pic(&pic,data,sizeof(data));
  pic.type=21;
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject picture type 21");
  pic.type=-1;
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject picture type -1");
  /*Non-printable MIME type.*/
  fill_pic(&pic,data,sizeof(data));
  pic.mime_type=(char *)"image/png\x1F";
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject control char in mime type");
  /*Empty MIME type.*/
  fill_pic(&pic,data,sizeof(data));
  pic.mime_type=(char *)"";
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject empty mime type");
  /*Description that is not valid UTF-8 (overlong encoding).*/
  fill_pic(&pic,data,sizeof(data));
  pic.description=(char *)"\xC0\x80";
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject invalid UTF-8 description");
  /*Truncated UTF-8 in description.*/
  pic.description=(char *)"\xE5\xB0";
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject truncated UTF-8 description");
  /*Data length set but no data.*/
  fill_pic(&pic,data,sizeof(data));
  pic.data=NULL;
  ret=opus_picture_tag_to_string(&pic,&tag);
  check(ret==OP_EINVAL,"reject data_length>0 with NULL data");
}

/*File-level error handling: writing to a missing file must fail with an
   error code, not a crash.*/
static void test_write_errors(void){
  OpusTags tags;
  int      ret;
  opus_tags_init(&tags);
  opus_tags_add(&tags,"TITLE","x");
  ret=op_write_tags("/nonexistent-dir/missing.opus",&tags);
  check(ret==OP_EREAD,"missing file rejected with OP_EREAD");
  opus_tags_clear(&tags);
}

int main(void){
  test_tags_roundtrip();
  test_tags_reject();
  test_picture_roundtrip();
  test_picture_reject();
  test_write_errors();
  printf("%i passed, %i failed\n",npass,nfail);
  return nfail==0?0:1;
}
