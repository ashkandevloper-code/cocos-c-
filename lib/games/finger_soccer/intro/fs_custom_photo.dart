import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import '../core/fs_custom_team.dart';

/// Decode a selected gallery image, crop its centre square, and encode a
/// bounded 256px PNG. The Storage bucket accepts only this one MIME/type/path;
/// no original filename or user-supplied URL is ever stored in a match.
Future<Uint8List> fsPrepareCustomPhoto(Uint8List source) async {
  if (source.isEmpty || source.length > 8 * 1024 * 1024) {
    throw const FormatException('photo_too_large');
  }
  final ui.Codec codec = await ui.instantiateImageCodec(source);
  final ui.FrameInfo frame;
  try {
    frame = await codec.getNextFrame();
  } finally {
    codec.dispose();
  }
  final ui.Image image = frame.image;
  try {
    final double side = math.min(image.width, image.height).toDouble();
    if (side <= 0) throw const FormatException('invalid_photo');
    final ui.Rect crop = ui.Rect.fromLTWH(
      (image.width - side) / 2, (image.height - side) / 2, side, side);
    final ui.PictureRecorder recorder = ui.PictureRecorder();
    ui.Canvas(recorder).drawImageRect(
      image, crop, const ui.Rect.fromLTWH(0, 0, 256, 256),
      ui.Paint()..filterQuality = ui.FilterQuality.high);
    final ui.Picture picture = recorder.endRecording();
    try {
      final ui.Image result = await picture.toImage(256, 256);
      try {
        final ByteData? data = await result.toByteData(format: ui.ImageByteFormat.png);
        if (data == null || data.lengthInBytes > kFsCustomMaxPngBytes) {
          throw const FormatException('photo_too_large');
        }
        final Uint8List png = data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes);
        if (!fsIsPng(png)) throw const FormatException('invalid_photo');
        return png;
      } finally {
        result.dispose();
      }
    } finally {
      picture.dispose();
    }
  } finally {
    image.dispose();
  }
}
