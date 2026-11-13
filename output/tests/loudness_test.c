/*Acceptance test for the loudness metadata parser and the optional gain.
  Built against the repo's own library via the CMake entry; see run_tests.sh.*/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <opusfile.h>

static int failures;

#define CHECK(cond, msg) do { \
  if (cond) { printf("  PASS: %s\n", msg); } \
  else { printf("  FAIL: %s\n", msg); failures++; } \
} while (0)

static OggOpusFile *open_of(const char *path) {
  int err = 0;
  OggOpusFile *of = op_open_file(path, &err);
  if (of == NULL) {
    printf("  FAIL: cannot open %s (err=%d)\n", path, err);
    failures++;
    return NULL;
  }
  return of;
}

static void test_valid(const char *path) {
  printf("[1] valid loudness tags round-trip: %s\n", path);
  OpusLoudness loud;
  OggOpusFile *of = open_of(path);
  if (of == NULL) return;
  OpusTags *tags = (OpusTags *)op_tags(of, 0);
  int rc = opus_tags_get_loudness(tags, &loud);
  CHECK(rc == 0, "parser returns success even while filling the struct");
  CHECK(loud.fields == (OP_LOUDNESS_TRACK_GAIN | OP_LOUDNESS_ALBUM_GAIN |
        OP_LOUDNESS_TRACK_PEAK | OP_LOUDNESS_ALBUM_PEAK),
        "all four fields are flagged present");
  CHECK(loud.track_gain_q8 == 512, "track gain q8 equals written 512 (2.000 dB)");
  CHECK(loud.album_gain_q8 == -256, "album gain q8 equals written -256 (-1.000 dB)");
  CHECK(fabs(loud.track_peak - 1.234567) < 1e-12,
        "track peak equals written 1.234567 (linear full scale)");
  CHECK(fabs(loud.album_peak - 0.987654) < 1e-12,
        "album peak equals written 0.987654 (linear full scale)");
  op_free(of);
}

static void test_missing(const char *path) {
  printf("[2] missing tags yield an empty structure, not an error: %s\n", path);
  OpusLoudness loud;
  OggOpusFile *of = open_of(path);
  if (of == NULL) return;
  OpusTags *tags = (OpusTags *)op_tags(of, 0);
  int rc = opus_tags_get_loudness(tags, &loud);
  CHECK(rc == 0, "missing tags still return success");
  CHECK(loud.fields == 0, "fields mask is zero");
  CHECK(loud.track_gain_q8 == 0 && loud.album_gain_q8 == 0,
        "gain values are zeroed");
  CHECK(loud.track_peak == 0.0 && loud.album_peak == 0.0,
        "peak values are zeroed");
  op_free(of);
}

static void test_invalid(const char *path) {
  printf("[3] invalid values are ignored, valid ones still parse: %s\n", path);
  OpusLoudness loud;
  OggOpusFile *of = open_of(path);
  if (of == NULL) return;
  OpusTags *tags = (OpusTags *)op_tags(of, 0);
  int rc = opus_tags_get_loudness(tags, &loud);
  CHECK(rc == 0, "invalid tags do not cause an error");
  CHECK((loud.fields & OP_LOUDNESS_TRACK_GAIN) == 0,
        "non-numeric R128_TRACK_GAIN ignored");
  CHECK((loud.fields & OP_LOUDNESS_ALBUM_GAIN) != 0
        && loud.album_gain_q8 == 300,
        "valid R128_ALBUM_GAIN=300 still parsed");
  CHECK((loud.fields & OP_LOUDNESS_TRACK_PEAK) == 0,
        "non-numeric track peak ignored");
  CHECK((loud.fields & OP_LOUDNESS_ALBUM_PEAK) == 0,
        "negative album peak ignored");
  op_free(of);
}

static long decode_all(const char *path, int apply_gain, float *out,
                       size_t cap) {
  int err = 0;
  OggOpusFile *of = op_open_file(path, &err);
  if (of == NULL) {
    printf("  FAIL: open %s err=%d\n", path, err);
    failures++;
    return -1;
  }
  op_set_gain_enabled(of, apply_gain);
  if (op_get_gain_enabled(of) != apply_gain) {
    printf("  FAIL: gain enable state mismatch\n");
    failures++;
  }
  long total = 0;
  for (;;) {
    int n = op_read_float_stereo(of, out + total * 2,
      (int)(cap - total) * 2);
    if (n < 0) {
      printf("  FAIL: decode error %d\n", n);
      failures++;
      break;
    }
    total += n;
    if (n == 0) break;
  }
  op_free(of);
  return total;
}

/*Decode with the R128 track gain selected (header gain + R128_TRACK_GAIN),
  to exercise the Q8 dB -> linear unit conversion through op_set_gain_offset.*/
static long decode_track_gain(const char *path, float *out, size_t cap) {
  int err = 0;
  OggOpusFile *of = op_open_file(path, &err);
  if (of == NULL) {
    printf("  FAIL: open %s err=%d\n", path, err);
    failures++;
    return -1;
  }
  if (op_set_gain_offset(of, OP_TRACK_GAIN, 0) != 0) {
    printf("  FAIL: op_set_gain_offset rejected OP_TRACK_GAIN\n");
    failures++;
  }
  long total = 0;
  for (;;) {
    int n = op_read_float_stereo(of, out + total * 2,
      (int)(cap - total) * 2);
    if (n < 0) {
      printf("  FAIL: decode error %d\n", n);
      failures++;
      break;
    }
    total += n;
    if (n == 0) break;
  }
  op_free(of);
  return total;
}

static void test_gain_off(const char *path_gain) {
  printf("[4] gain disabled -> samples identical across two runs and to a\n"
         "    zero-header-gain decode; enabled output differs: %s\n",
         path_gain);
  enum { CAP = 48000 };
  static float a[CAP * 2], b[CAP * 2], on[CAP * 2];
  long na = decode_all(path_gain, 0, a, CAP);
  long nb = decode_all(path_gain, 0, b, CAP);
  long non = decode_all(path_gain, 1, on, CAP);
  CHECK(na > 0 && na == nb && nb == non, "all runs decoded the same length");
  CHECK(memcmp(a, b, (size_t)na * 2 * sizeof(float)) == 0,
        "gain off: two runs are sample-for-sample identical (zero diff)");
  int changed = 0;
  for (long i = 0; i < non * 2; i++) if (a[i] != on[i]) { changed = 1; break; }
  CHECK(changed, "gain on changes the samples (768 q8 header + 512 q8 tag)");
  /*Default gain type is OP_HEADER_GAIN: only the 768 q8 header gain applies.
    Float dB->linear conversion is pow(10, gain_q8/(20*256)).*/
  double expected = pow(10.0, 768.0 / (20.0 * 256.0));
  double ratio = on[200] / a[200];
  CHECK(fabs(ratio - expected) < 2e-5,
        "applied scale matches dB->linear unit conversion");

  /*Selecting OP_TRACK_GAIN adds the 512 q8 R128 tag gain on top.*/
  static float tr[CAP * 2];
  long ntr = decode_track_gain(path_gain, tr, CAP);
  CHECK(ntr == na, "track-gain decode produced the same sample count");
  double expected_track = pow(10.0, (768.0 + 512.0) / (20.0 * 256.0));
  double track_ratio = tr[200] / a[200];
  CHECK(fabs(track_ratio - expected_track) < 2e-5,
        "R128 track gain scale matches dB->linear unit conversion");
}

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: %s valid.opus notags.opus invalid.opus "
            "with_gain_head.opus\n", argv[0]);
    return 2;
  }
  test_valid(argv[1]);
  test_missing(argv[2]);
  test_invalid(argv[3]);
  test_gain_off(argv[4]);
  printf(failures ? "\nRESULT: %d FAILURE(S)\n" : "\nRESULT: all checks passed\n",
         failures);
  return failures ? 1 : 0;
}
