/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Reads a vertex capture (vertex_capture.bin, written by the core when the
 * "Capture Vertex Inputs" option is switched on:
 * src/video_core/renderer_vulkan/vk_vertex_capture.h) and says, for each draw
 * with vertices thrown far from the rest of their mesh, where those vertices
 * came from:
 *
 *   game     the game's memory holds the same bad values: the game's own code
 * (its CPU side: animation, cloth, physics) computed them, or a CPU instruction
 * is emulated wrong gpu      the GPU wrote them (compute shader output) and the
 * game's memory differs: the shader, or what it was given, is wrong clobber the
 * range is the GPU's to write, yet holds what the game's memory has: the GPU's
 *            bytes were replaced by an upload (the buffer cache; readbacks off)
 *   upload   the GPU was given bytes the game's memory does not have, in a
 * range the GPU never wrote: the buffer cache gave the draw stale or wrong
 * bytes
 *
 *   cc -O2 -o vertex_capture_report tools/vertex_capture_report.c -lm
 *   ./vertex_capture_report vertex_capture.bin
 *   ./vertex_capture_report --selftest */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned int u32;
#if defined(_MSC_VER)
typedef unsigned __int64 u64;
#else
__extension__ typedef unsigned long long u64;
#endif

#define CHUNK_DRAW 1u
#define CHUNK_GUEST 2u
#define CHUNK_HOST 3u

#define BINDING_GPU_WRITTEN 1u
#define BINDING_PER_INSTANCE 2u
#define BINDING_STREAM 4u

typedef struct binding {
  u32 semantic, format, stride, flags;
  u64 address, size;
} binding_t;

typedef struct draw {
  u32 primitive, num_indices, num_instances, count;
  const binding_t *bindings;
} draw_t;

typedef struct data {
  u32 type, draw;
  u64 address, size;
  const u8 *bytes;
} data_t;

static draw_t *draws;
static u32 num_draws;
static data_t *datas;
static u32 num_datas;

/* Components and bytes of each of a float format (VkFormat), or 0 */
static int float_format(u32 format, int *width) {
  switch (format) {
  case 83:
    *width = 2;
    return 2; /* R16G16_SFLOAT */
  case 90:
    *width = 2;
    return 3; /* R16G16B16_SFLOAT */
  case 97:
    *width = 2;
    return 4; /* R16G16B16A16_SFLOAT */
  case 103:
    *width = 4;
    return 2; /* R32G32_SFLOAT */
  case 106:
    *width = 4;
    return 3; /* R32G32B32_SFLOAT */
  case 109:
    *width = 4;
    return 4; /* R32G32B32A32_SFLOAT */
  default:
    return 0;
  }
}

static float half_to_float(unsigned h) {
  const unsigned sign = h >> 15, exponent = (h >> 10) & 31u,
                 mantissa = h & 1023u;
  double value;
  if (exponent == 0) {
    value = ldexp((double)mantissa, -24);
  } else if (exponent == 31) {
    value = mantissa ? HUGE_VAL - HUGE_VAL : HUGE_VAL;
  } else {
    value = ldexp((double)(mantissa + 1024u), (int)exponent - 25);
  }
  return (float)(sign ? -value : value);
}

static float component(const u8 *at, int width) {
  float f;
  if (width == 4) {
    memcpy(&f, at, 4);
    return f;
  }
  return half_to_float((unsigned)at[0] | ((unsigned)at[1] << 8));
}

/* The newest bytes of @type recorded up to draw @draw that cover the range */
static const u8 *find_data(u32 type, u32 draw, u64 address, u64 size) {
  u32 i;
  for (i = num_datas; i-- > 0;) {
    const data_t *d = &datas[i];
    if (d->type == type && d->draw <= draw && address >= d->address &&
        address + size <= d->address + d->size) {
      return d->bytes + (address - d->address);
    }
  }
  return NULL;
}

static int compare_floats(const void *a, const void *b) {
  const float x = *(const float *)a, y = *(const float *)b;
  return (x > y) - (x < y);
}

static int parse(const u8 *file, size_t size) {
  size_t at = 8;
  u32 d = 0, n = 0;
  if (size < 8 || memcmp(file, "PS4VCAP1", 8) != 0) {
    return 0;
  }
  /* Two passes: count, then fill */
  for (at = 8; at + 16 <= size;) {
    u32 type;
    u64 chunk;
    memcpy(&type, file + at, 4);
    memcpy(&chunk, file + at + 8, 8);
    if (chunk > size - at - 16) {
      break; /* cut short: what is whole is used */
    }
    if (type == CHUNK_DRAW) {
      ++num_draws;
    } else {
      ++num_datas;
    }
    at += 16 + (size_t)chunk;
  }
  draws = (draw_t *)calloc(num_draws ? num_draws : 1, sizeof(*draws));
  datas = (data_t *)calloc(num_datas ? num_datas : 1, sizeof(*datas));
  for (at = 8; at + 16 <= size;) {
    u32 type, draw;
    u64 chunk;
    const u8 *body = file + at + 16;
    memcpy(&type, file + at, 4);
    memcpy(&draw, file + at + 4, 4);
    memcpy(&chunk, file + at + 8, 8);
    if (chunk > size - at - 16) {
      break;
    }
    if (type == CHUNK_DRAW && chunk >= 16) {
      memcpy(&draws[d], body, 16);
      if ((u64)draws[d].count * sizeof(binding_t) > chunk - 16) {
        draws[d].count = 0;
      }
      draws[d].bindings = (const binding_t *)(body + 16);
      ++d;
    } else if (type != CHUNK_DRAW && chunk >= 8) {
      datas[n].type = type;
      datas[n].draw = draw;
      memcpy(&datas[n].address, body, 8);
      datas[n].size = chunk - 8;
      datas[n].bytes = body + 8;
      ++n;
    }
    at += 16 + (size_t)chunk;
  }
  num_draws = d;
  num_datas = n;
  return 1;
}

static u32 verdicts[4]; /* game, gpu, clobber, upload */
static const char *const verdict_names[4] = {"game", "gpu", "clobber",
                                             "upload"};

static void report(int verbose) {
  u32 di, bi, checked = 0;
  for (di = 0; di < num_draws; ++di) {
    const draw_t *draw = &draws[di];
    for (bi = 0; bi < draw->count; ++bi) {
      binding_t b;
      const u8 *host, *guest;
      float *values, median[4], extent[4];
      int width = 0, comps, c, verdict;
      u32 n, v, bad = 0, same = 0, guest_bad = 0, first_bad = 0;
      memcpy(&b, &draw->bindings[bi], sizeof(b));
      comps = float_format(b.format, &width);
      if (comps < 3 || (b.flags & BINDING_PER_INSTANCE) || b.stride == 0 ||
          b.size == 0 || b.stride < (u32)(comps * width)) {
        continue;
      }
      host = find_data(CHUNK_HOST, di, b.address, b.size);
      guest = find_data(CHUNK_GUEST, di, b.address, b.size);
      if (!host || !guest) {
        continue;
      }
      n = (u32)((b.size - (u64)(comps * width)) / b.stride) + 1;
      if (n < 8) {
        continue;
      }
      ++checked;
      /* Where the mesh is: each component's median, and the spread of its
       * middle 90% */
      values = (float *)malloc((size_t)n * sizeof(float));
      for (c = 0; c < 3; ++c) {
        u32 finite = 0;
        for (v = 0; v < n; ++v) {
          const float f =
              component(host + (size_t)v * b.stride + c * width, width);
          if (f == f && fabs(f) < 1e30) {
            values[finite++] = f;
          }
        }
        if (finite == 0) {
          median[c] = 0;
          extent[c] = 1;
          continue;
        }
        qsort(values, finite, sizeof(float), compare_floats);
        median[c] = values[finite / 2];
        extent[c] = values[finite - 1 - finite / 20] - values[finite / 20];
      }
      free(values);
      {
        float span = extent[0];
        if (extent[1] > span)
          span = extent[1];
        if (extent[2] > span)
          span = extent[2];
        if (span < 1e-3f)
          span = 1e-3f;
        for (v = 0; v < n; ++v) {
          const u8 *h = host + (size_t)v * b.stride;
          const u8 *g = guest + (size_t)v * b.stride;
          int out = 0, guest_out = 0;
          for (c = 0; c < 3; ++c) {
            const float f = component(h + c * width, width);
            const float gf = component(g + c * width, width);
            if (!(f == f) || fabs(f - median[c]) > 50.0f * span)
              out = 1;
            if (!(gf == gf) || fabs(gf - median[c]) > 50.0f * span)
              guest_out = 1;
          }
          if (!out) {
            continue;
          }
          if (!bad)
            first_bad = v;
          ++bad;
          if (memcmp(h, g, (size_t)(comps * width)) == 0)
            ++same;
          if (guest_out)
            ++guest_bad;
        }
      }
      if (!bad) {
        continue;
      }
      if (b.flags & BINDING_GPU_WRITTEN) {
        verdict = same * 2 >= bad ? 2 : 1;
      } else {
        verdict = same * 2 >= bad ? 0 : 3;
      }
      ++verdicts[verdict];
      if (verbose) {
        printf("draw %u input %u: %u of %u vertices thrown (first %u), %s%s; "
               "game's memory "
               "has the same for %u, bad for %u -> %s\n",
               di, b.semantic, bad, n, first_bad,
               (b.flags & BINDING_GPU_WRITTEN) ? "GPU-written range"
                                               : "not GPU-written",
               (b.flags & BINDING_STREAM) ? ", stream buffer" : "", same,
               guest_bad, verdict_names[verdict]);
      }
    }
  }
  printf("%u draws, %u vertex inputs checked\n", num_draws, checked);
  printf("thrown vertices: game %u, gpu %u, clobber %u, upload %u\n",
         verdicts[0], verdicts[1], verdicts[2], verdicts[3]);
  if (!(verdicts[0] | verdicts[1] | verdicts[2] | verdicts[3])) {
    printf("no thrown vertices in this capture: capture while the broken mesh "
           "is on screen\n");
  } else if (verdicts[2] >= verdicts[0] && verdicts[2] >= verdicts[1] &&
             verdicts[2] >= verdicts[3]) {
    printf("=> the GPU's vertices were replaced by an upload of the game's "
           "memory\n");
  } else if (verdicts[0] >= verdicts[1] && verdicts[0] >= verdicts[3]) {
    printf("=> the game's own code produced them (CPU side: animation, cloth, "
           "physics)\n");
  } else if (verdicts[1] >= verdicts[3]) {
    printf("=> a compute shader produced them (the shader or its inputs)\n");
  } else {
    printf("=> the buffer cache gave the draw bytes the game's memory does not "
           "have\n");
  }
}

/* ---- self test: a capture with one mesh of each kind ---- */

static u8 *out;
static size_t out_size;

static void put(const void *bytes, size_t size) {
  out = (u8 *)realloc(out, out_size + size);
  memcpy(out + out_size, bytes, size);
  out_size += size;
}

static void put_chunk(u32 type, u32 draw, const void *head, u64 head_size,
                      const void *body, u64 body_size) {
  const u64 size = head_size + body_size;
  put(&type, 4);
  put(&draw, 4);
  put(&size, 8);
  put(head, (size_t)head_size);
  put(body, (size_t)body_size);
}

static void put_mesh(u32 draw, u32 flags, int host_bad, int guest_bad) {
  float host[64 * 3], guest[64 * 3];
  binding_t b;
  u32 head[4];
  u64 address = 0x100000ull * (draw + 1);
  int i;
  for (i = 0; i < 64 * 3; ++i) {
    host[i] = guest[i] = (float)(i % 17) * 0.1f;
  }
  if (host_bad) {
    host[30] = 1e9f;
    host[60] = -4e7f;
  }
  if (guest_bad) {
    guest[30] = host_bad ? host[30] : 7e8f;
    guest[60] = host_bad ? host[60] : 7e8f;
  }
  if (flags & BINDING_GPU_WRITTEN && !guest_bad && host_bad) {
    guest[30] =
        0; /* never written back: the game's memory has something else */
  }
  b.semantic = 0;
  b.format = 106;
  b.stride = 12;
  b.flags = flags;
  b.address = address;
  b.size = sizeof(host);
  head[0] = 4;
  head[1] = 64;
  head[2] = 1;
  head[3] = 1;
  put_chunk(CHUNK_DRAW, draw, head, sizeof(head), &b, sizeof(b));
  put_chunk(CHUNK_HOST, draw, &address, 8, host, sizeof(host));
  put_chunk(CHUNK_GUEST, draw, &address, 8, guest, sizeof(guest));
}

static int selftest(void) {
  put("PS4VCAP1", 8);
  put_mesh(0, 0, 0, 0);                   /* sound */
  put_mesh(1, 0, 1, 1);                   /* game */
  put_mesh(2, BINDING_GPU_WRITTEN, 1, 0); /* gpu */
  put_mesh(3, BINDING_GPU_WRITTEN, 1, 1); /* clobber */
  put_mesh(4, 0, 1, 0);                   /* upload */
  put_mesh(5, BINDING_GPU_WRITTEN, 0,
           1); /* sound: only the game's stale memory is off */
  if (!parse(out, out_size)) {
    printf("FAIL parse\n");
    return 1;
  }
  report(1);
  if (num_draws != 6 || verdicts[0] != 1 || verdicts[1] != 1 ||
      verdicts[2] != 1 || verdicts[3] != 1) {
    printf("FAIL verdicts\n");
    return 1;
  }
  printf("vertex_capture_report: self test passed\n");
  return 0;
}

int main(int argc, char **argv) {
  FILE *f;
  u8 *file;
  long size;
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
    return selftest();
  }
  if (argc != 2) {
    printf("usage: %s vertex_capture.bin | --selftest\n", argv[0]);
    return 2;
  }
  f = fopen(argv[1], "rb");
  if (!f) {
    printf("cannot open %s\n", argv[1]);
    return 2;
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  file = (u8 *)malloc((size_t)size + 1);
  if (!file || fread(file, 1, (size_t)size, f) != (size_t)size) {
    printf("cannot read %s\n", argv[1]);
    return 2;
  }
  fclose(f);
  if (!parse(file, (size_t)size)) {
    printf("%s is not a vertex capture\n", argv[1]);
    return 2;
  }
  report(1);
  return 0;
}
