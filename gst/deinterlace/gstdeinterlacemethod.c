/*
 * GStreamer
 * Copyright (C) 2008-2010 Sebastian Dröge <slomo@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <liboil/liboil.h>
#include <liboil/liboilfunction.h>

#include "gstdeinterlacemethod.h"

G_DEFINE_ABSTRACT_TYPE (GstDeinterlaceMethod, gst_deinterlace_method,
    GST_TYPE_OBJECT);

gboolean
gst_deinterlace_method_supported (GType type, GstVideoFormat format, gint width,
    gint height)
{
  GstDeinterlaceMethodClass *klass =
      GST_DEINTERLACE_METHOD_CLASS (g_type_class_ref (type));
  gboolean ret;

  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    ret = TRUE;
  else
    ret = klass->supported (klass, format, width, height);
  g_type_class_unref (klass);

  return ret;
}

static gboolean
gst_deinterlace_method_supported_impl (GstDeinterlaceMethodClass * klass,
    GstVideoFormat format, gint width, gint height)
{
  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      return (klass->deinterlace_frame_yuy2 != NULL);
    case GST_VIDEO_FORMAT_YVYU:
      return (klass->deinterlace_frame_yvyu != NULL);
    default:
      return FALSE;
  }
}

void
gst_deinterlace_method_setup (GstDeinterlaceMethod * self,
    GstVideoFormat format, gint width, gint height)
{
  GstDeinterlaceMethodClass *klass = GST_DEINTERLACE_METHOD_GET_CLASS (self);

  klass->setup (self, format, width, height);
}

static void
gst_deinterlace_method_setup_impl (GstDeinterlaceMethod * self,
    GstVideoFormat format, gint width, gint height)
{
  gint i;
  GstDeinterlaceMethodClass *klass = GST_DEINTERLACE_METHOD_GET_CLASS (self);

  self->format = format;
  self->frame_width = width;
  self->frame_height = height;

  self->deinterlace_frame = NULL;

  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return;

  for (i = 0; i < 4; i++) {
    self->width[i] = gst_video_format_get_component_width (format, i, width);
    self->height[i] = gst_video_format_get_component_height (format, i, height);
    self->offset[i] =
        gst_video_format_get_component_offset (format, i, width, height);
    self->row_stride[i] = gst_video_format_get_row_stride (format, i, width);
    self->pixel_stride[i] = gst_video_format_get_pixel_stride (format, i);
  }

  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      self->deinterlace_frame = klass->deinterlace_frame_yuy2;
      break;
    case GST_VIDEO_FORMAT_YVYU:
      self->deinterlace_frame = klass->deinterlace_frame_yvyu;
      break;
    default:
      self->deinterlace_frame = NULL;
      break;
  }
}

static void
gst_deinterlace_method_class_init (GstDeinterlaceMethodClass * klass)
{
  klass->setup = gst_deinterlace_method_setup_impl;
  klass->supported = gst_deinterlace_method_supported_impl;
}

static void
gst_deinterlace_method_init (GstDeinterlaceMethod * self)
{
  self->format = GST_VIDEO_FORMAT_UNKNOWN;
}

void
gst_deinterlace_method_deinterlace_frame (GstDeinterlaceMethod * self,
    const GstDeinterlaceField * history, guint history_count,
    GstBuffer * outbuf)
{
  g_assert (self->deinterlace_frame != NULL);
  self->deinterlace_frame (self, history, history_count, outbuf);
}

gint
gst_deinterlace_method_get_fields_required (GstDeinterlaceMethod * self)
{
  GstDeinterlaceMethodClass *klass = GST_DEINTERLACE_METHOD_GET_CLASS (self);

  return klass->fields_required;
}

gint
gst_deinterlace_method_get_latency (GstDeinterlaceMethod * self)
{
  GstDeinterlaceMethodClass *klass = GST_DEINTERLACE_METHOD_GET_CLASS (self);

  return klass->latency;
}

G_DEFINE_ABSTRACT_TYPE (GstDeinterlaceSimpleMethod,
    gst_deinterlace_simple_method, GST_TYPE_DEINTERLACE_METHOD);

static gboolean
gst_deinterlace_simple_method_supported (GstDeinterlaceMethodClass * mklass,
    GstVideoFormat format, gint width, gint height)
{
  GstDeinterlaceSimpleMethodClass *klass =
      GST_DEINTERLACE_SIMPLE_METHOD_CLASS (mklass);

  if (!GST_DEINTERLACE_METHOD_CLASS
      (gst_deinterlace_simple_method_parent_class)->supported (mklass, format,
          width, height))
    return FALSE;

  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      return (klass->interpolate_scanline_yuy2 != NULL
          && klass->copy_scanline_yuy2 != NULL);
    case GST_VIDEO_FORMAT_YVYU:
      return (klass->interpolate_scanline_yvyu != NULL
          && klass->copy_scanline_yvyu != NULL);
    default:
      return FALSE;
  }
}

static void
    gst_deinterlace_simple_method_interpolate_scanline_packed
    (GstDeinterlaceSimpleMethod * self, guint8 * out,
    const GstDeinterlaceScanlineData * scanlines)
{
  oil_memcpy (out, scanlines->m1, self->parent.row_stride[0]);
}

static void
gst_deinterlace_simple_method_copy_scanline_packed (GstDeinterlaceSimpleMethod *
    self, guint8 * out, const GstDeinterlaceScanlineData * scanlines)
{
  oil_memcpy (out, scanlines->m0, self->parent.row_stride[0]);
}

static void
gst_deinterlace_simple_method_deinterlace_frame_packed (GstDeinterlaceMethod *
    method, const GstDeinterlaceField * history, guint history_count,
    GstBuffer * outbuf)
{
  GstDeinterlaceSimpleMethod *self = GST_DEINTERLACE_SIMPLE_METHOD (method);
  GstDeinterlaceMethodClass *dm_class = GST_DEINTERLACE_METHOD_GET_CLASS (self);
  GstDeinterlaceScanlineData scanlines;
  guint8 *out = GST_BUFFER_DATA (outbuf);
  guint8 *field0 = NULL, *field1 = NULL, *field2 = NULL, *field3 = NULL;
  gint cur_field_idx = history_count - dm_class->fields_required;
  guint cur_field_flags = history[cur_field_idx].flags;
  gint line;
  gint field_height = self->parent.frame_height / 2;
  gint row_stride = self->parent.row_stride[0];
  gint field_stride = self->parent.row_stride[0] * 2;

  g_assert (self->interpolate_scanline_packed != NULL);
  g_assert (self->copy_scanline_packed != NULL);

  field0 = GST_BUFFER_DATA (history[cur_field_idx].buf);
  if (history[cur_field_idx].flags & PICTURE_INTERLACED_BOTTOM)
    field0 += row_stride;

  g_return_if_fail (dm_class->fields_required <= 4);

  if (dm_class->fields_required >= 2) {
    field1 = GST_BUFFER_DATA (history[cur_field_idx + 1].buf);
    if (history[cur_field_idx + 1].flags & PICTURE_INTERLACED_BOTTOM)
      field1 += row_stride;
  }

  if (dm_class->fields_required >= 3) {
    field2 = GST_BUFFER_DATA (history[cur_field_idx + 2].buf);
    if (history[cur_field_idx + 2].flags & PICTURE_INTERLACED_BOTTOM)
      field2 += row_stride;
  }

  if (dm_class->fields_required >= 4) {
    field3 = GST_BUFFER_DATA (history[cur_field_idx + 3].buf);
    if (history[cur_field_idx + 3].flags & PICTURE_INTERLACED_BOTTOM)
      field3 += row_stride;
  }


  if (cur_field_flags == PICTURE_INTERLACED_BOTTOM) {
    /* double the first scanline of the bottom field */
    oil_memcpy (out, field0, row_stride);
    out += row_stride;
  }

  oil_memcpy (out, field0, row_stride);
  out += row_stride;

  for (line = 2; line <= field_height; line++) {

    memset (&scanlines, 0, sizeof (scanlines));
    scanlines.bottom_field = (cur_field_flags == PICTURE_INTERLACED_BOTTOM);

    /* interp. scanline */
    scanlines.t0 = field0;
    scanlines.b0 = field0 + field_stride;

    if (field1 != NULL) {
      scanlines.tt1 = field1;
      scanlines.m1 = field1 + field_stride;
      scanlines.bb1 = field1 + field_stride * 2;
      field1 += field_stride;
    }

    if (field2 != NULL) {
      scanlines.t2 = field2;
      scanlines.b2 = field2 + field_stride;
    }

    if (field3 != NULL) {
      scanlines.tt3 = field3;
      scanlines.m3 = field3 + field_stride;
      scanlines.bb3 = field3 + field_stride * 2;
      field3 += field_stride;
    }

    /* set valid data for corner cases */
    if (line == 2) {
      scanlines.tt1 = scanlines.bb1;
      scanlines.tt3 = scanlines.bb3;
    } else if (line == field_height) {
      scanlines.bb1 = scanlines.tt1;
      scanlines.bb3 = scanlines.tt3;
    }

    self->interpolate_scanline_packed (self, out, &scanlines);
    out += row_stride;

    memset (&scanlines, 0, sizeof (scanlines));
    scanlines.bottom_field = (cur_field_flags == PICTURE_INTERLACED_BOTTOM);

    /* copy a scanline */
    scanlines.tt0 = field0;
    scanlines.m0 = field0 + field_stride;
    scanlines.bb0 = field0 + field_stride * 2;
    field0 += field_stride;

    if (field1 != NULL) {
      scanlines.t1 = field1;
      scanlines.b1 = field1 + field_stride;
    }

    if (field2 != NULL) {
      scanlines.tt2 = field2;
      scanlines.m2 = field2 + field_stride;
      scanlines.bb2 = field2 + field_stride * 2;
      field2 += field_stride;
    }

    if (field3 != NULL) {
      scanlines.t3 = field3;
      scanlines.b3 = field3 + field_stride;
    }

    /* set valid data for corner cases */
    if (line == field_height) {
      scanlines.bb0 = scanlines.tt0;
      scanlines.b1 = scanlines.t1;
      scanlines.bb2 = scanlines.tt2;
      scanlines.b3 = scanlines.t3;
    }

    self->copy_scanline_packed (self, out, &scanlines);
    out += row_stride;
  }

  if (cur_field_flags == PICTURE_INTERLACED_TOP) {
    /* double the last scanline of the top field */
    oil_memcpy (out, field0, row_stride);
  }
}

static void
gst_deinterlace_simple_method_setup (GstDeinterlaceMethod * method,
    GstVideoFormat format, gint width, gint height)
{
  GstDeinterlaceSimpleMethod *self = GST_DEINTERLACE_SIMPLE_METHOD (method);
  GstDeinterlaceSimpleMethodClass *klass =
      GST_DEINTERLACE_SIMPLE_METHOD_GET_CLASS (self);

  GST_DEINTERLACE_METHOD_CLASS
      (gst_deinterlace_simple_method_parent_class)->setup (method, format,
      width, height);

  self->interpolate_scanline_packed = NULL;
  self->copy_scanline_packed = NULL;

  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return;

  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      self->interpolate_scanline_packed = klass->interpolate_scanline_yuy2;
      self->copy_scanline_packed = klass->copy_scanline_yuy2;
      break;
    case GST_VIDEO_FORMAT_YVYU:
      self->interpolate_scanline_packed = klass->interpolate_scanline_yvyu;
      self->copy_scanline_packed = klass->copy_scanline_yvyu;
      break;
    default:
      break;
  }
}

static void
gst_deinterlace_simple_method_class_init (GstDeinterlaceSimpleMethodClass *
    klass)
{
  GstDeinterlaceMethodClass *dm_class = (GstDeinterlaceMethodClass *) klass;

  dm_class->deinterlace_frame_yuy2 =
      gst_deinterlace_simple_method_deinterlace_frame_packed;
  dm_class->deinterlace_frame_yvyu =
      gst_deinterlace_simple_method_deinterlace_frame_packed;
  dm_class->fields_required = 2;
  dm_class->setup = gst_deinterlace_simple_method_setup;
  dm_class->supported = gst_deinterlace_simple_method_supported;

  klass->interpolate_scanline_yuy2 =
      gst_deinterlace_simple_method_interpolate_scanline_packed;
  klass->copy_scanline_yuy2 =
      gst_deinterlace_simple_method_copy_scanline_packed;
  klass->interpolate_scanline_yvyu =
      gst_deinterlace_simple_method_interpolate_scanline_packed;
  klass->copy_scanline_yvyu =
      gst_deinterlace_simple_method_copy_scanline_packed;
}

static void
gst_deinterlace_simple_method_init (GstDeinterlaceSimpleMethod * self)
{
}