/* Copyright (c) 2017 LiteSpeed Technologies Inc.  See LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "lsquic_types.h"
#include "lsquic_alarmset.h"
#include "lsquic_parse.h"
#include "lsquic_packet_common.h"
#include "lsquic_packet_in.h"
#include "lsquic.h"

struct test {
    const char     *name;
    int             lineno;
    const struct parse_funcs *
                    pf;
    const unsigned char
                    buf[0x100];    /* Large enough for our needs */
    size_t          buf_sz;        /* # of stream frame bytes in `buf' */
    size_t          rem_packet_sz; /* # of bytes remaining in the packet,
                                    * starting at the beginning of the
                                    * stream frame.
                                    */
    stream_frame_t  frame;         /* Expected values */
    int             should_succeed;
};

static const struct test tests[] = {

    /*
     * Litte-endian tests;
     */
    {   "Balls to the wall: every possible bit is set",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x1C | 0x3,
          0x10, 0x02, 0x00, 0x00,                           /* Stream ID */
          0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 2    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Balls to the wall #2: every possible bit is set, except FIN",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x1C | 0x3,
          0x10, 0x02, 0x00, 0x00,                           /* Stream ID */
          0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 2    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Data length is zero",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x00 | 0x1C | 0x3,
          0x10, 0x02, 0x00, 0x00,                           /* Stream ID */
          0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 0    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x200 - (1 + 8 + 4),
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Stream ID length is 1",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x1C | 0x0,
          0xF0,                                             /* Stream ID */
          0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 2    + 8    + 1,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "All bits are zero save offset length",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x00 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 0    + 2    + 1,
        0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #1",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x00 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02,                                       /* Offset */
        },
          1           + 0    + 2    + 1,
          4,    /* Same as buffer size: in the absense of explicit data
                 * length in the header, this would mean that data
                 * length is zero.
                 */
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #2",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02,                                       /* Offset */
          0x00, 0x00,
        },
          1           + 2    + 2    + 1,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #3",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02,                                       /* Offset */
          0x00, 0x00,
        },
          1           + 2    + 2    + 1,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x0,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Check data bounds #1",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02,                                       /* Offset */
          0xFA, 0x01,                                       /* Data length */
        },
          1           + 2    + 2    + 1,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Check data bounds #2",
        __LINE__,
        select_pf_by_ver(LSQVER_037),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x55, 0x02,                                       /* Offset */
          0xFB, 0x01,    /* <---   One byte too many */
        },
          1           + 2    + 2    + 1,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    /*
     * Big-endian tests
     */
    {   "Balls to the wall: every possible bit is set",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x1C | 0x3,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1           + 2    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Balls to the wall #2: every possible bit is set, except FIN",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x1C | 0x3,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1           + 2    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Data length is zero",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x00 | 0x1C | 0x3,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0xC4, 0x01,                                       /* Data length: note this does not matter */
        },
          1           + 0    + 8    + 4,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x200 - (1 + 8 + 4),
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Stream ID length is 1",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x1C | 0x0,
          0xF0,                                             /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1           + 2    + 8    + 1,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "All bits are zero save offset length",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x00 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1           + 0    + 2    + 1,
        0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #1",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x00 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
        },
          1           + 0    + 2    + 1,
          4,    /* Same as buffer size: in the absense of explicit data
                 * length in the header, this would mean that data
                 * length is zero.
                 */
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #2",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x00, 0x00,
        },
          1           + 2    + 2    + 1,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #3",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x40 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x00, 0x00,
        },
          1           + 2    + 2    + 1,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x0,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Check data bounds #1",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x01, 0xFA,                                       /* Data length */
        },
          1           + 2    + 2    + 1,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Check data bounds #2",
        __LINE__,
        select_pf_by_ver(LSQVER_039),
      /*  1      f      d      ooo    ss            1fdoooss */
      /*  TYPE   FIN    DLEN   OLEN   SLEN  */
        { 0x80 | 0x00 | 0x20 | 0x04 | 0x0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x01, 0xFB,    /* <---   One byte too many */
        },
          1           + 2    + 2    + 1,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    /*
     * GQUIC IETF tests
     */
    {   "Balls to the wall: every possible bit is set",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 1 << 5 | 3 << 3 | 3 << 1 | 1,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1             + 4      + 8      + 2,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Balls to the wall #2: every possible bit is set, except FIN",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 3 << 3 | 3 << 1 | 1,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1             + 4      + 8      + 2,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Data length is zero",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 1 << 5 | 3 << 3 | 3 << 1 | 0,
          0x00, 0x00, 0x02, 0x10,                           /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0xC4, 0x01,                                       /* Data length: note this does not matter */
        },
          1             + 4      + 8      + 0,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0x210,
            .data_frame.df_size = 0x200 - (1 + 8 + 4),
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Stream ID length is 1",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 1 << 5 | 0 << 3 | 3 << 1 | 1,
          0xF0,                                             /* Stream ID */
          0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,   /* Offset */
          0x01, 0xC4,                                       /* Data length */
        },
          1             + 1      + 8      + 2,
        0x200,
        {   .data_frame.df_offset      = 0x0807060504030201UL,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1C4,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "All bits are zero save offset length",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 0 << 3 | 1 << 1 | 0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* Offset */
          0xC4, 0x01,                                       /* Data length */
        },
          1             + 1      + 2      + 0,
        0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #1",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 0 << 3 | 1 << 1 | 0,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
        },
          1             + 1      + 2      + 0,
          4,    /* Same as buffer size: in the absense of explicit data
                 * length in the header, this would mean that data
                 * length is zero.
                 */
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #2",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 0 << 3 | 1 << 1 | 1,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x00, 0x00,
        },
          1             + 1      + 2      + 2,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x200 - 4,
            .data_frame.df_fin         = 0,
        },
        0,
    },

    {   "Sanity check: either FIN must be set or data length is not zero #3",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 1 << 5 | 0 << 3 | 1 << 1 | 1,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x00, 0x00,
        },
          1             + 1      + 2      + 2,
          200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x0,
            .data_frame.df_fin         = 1,
        },
        1,
    },

    {   "Check data bounds #1",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 0 << 3 | 1 << 1 | 1,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x01, 0xFA,                                       /* Data length */
        },
          1             + 1      + 2      + 2,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        1,
    },

    {   "Check data bounds #2",
        __LINE__,
        select_pf_by_ver(LSQVER_041),
      /*  11     F        SS       OO       D            11FSSOOD */
      /*  TYPE   FIN      SLEN     OLEN     DLEN  */
        { 0xC0 | 0 << 5 | 0 << 3 | 1 << 1 | 1,
          0xF0,                                             /* Stream ID */
          0x02, 0x55,                                       /* Offset */
          0x01, 0xFB,    /* <---   One byte too many */
        },
          1             + 1      + 2      + 2,
          0x200,
        {   .data_frame.df_offset      = 0x255,
            .stream_id   = 0xF0,
            .data_frame.df_size = 0x1FA,
            .data_frame.df_fin         = 0,
        },
        0,
    },

};


static void
run_test (const struct test *test)
{
    stream_frame_t frame;
    memset(&frame, 0x7A, sizeof(frame));

    int len = test->pf->pf_parse_stream_frame(test->buf, test->rem_packet_sz, &frame);

    if (test->should_succeed) {
        /* Check parser operation */
        assert(("Parsed correct number of bytes", (size_t) len == test->buf_sz + test->frame.data_frame.df_size));
        assert(("Stream ID is correct", frame.stream_id == test->frame.stream_id));
        assert(("Data length is correct", frame.data_frame.df_size == test->frame.data_frame.df_size));
        assert(("Offset is correct", frame.data_frame.df_offset == test->frame.data_frame.df_offset));
        assert(("FIN is correct", frame.data_frame.df_fin == test->frame.data_frame.df_fin));

        /* Check that initialization of other fields occurred correctly: */
        assert(0 == frame.packet_in);
        assert(0 == frame.data_frame.df_read_off);
    }
    else
    {
        assert(("This test should fail", len < 0));
    }
}


int
main (void)
{
    unsigned i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
        run_test(&tests[i]);
    return 0;
}
