import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:nexora/core/economy/gem_shop.dart';
import 'package:nexora/core/online/online_service.dart';

import '../core/bm_engine.dart';
import '../core/fs_custom_team.dart';
import '../core/fs_custom_team_photo.dart';

/// All team assets are bundled except a purchased photo. For the account that
/// uploaded it, use the disk-backed copy in offline matches; a public URL is
/// derived from the validated team ID for anyone else. Never render a peer URL.
Widget fsTeamArt(String asset, {BoxFit fit = BoxFit.contain, int? cacheWidth}) {
  final String? ownId = GemShop.instance.customTeamId;
  if (ownId != null && bmTeam(ownId).asset == asset) {
    final Uint8List? local = FsCustomTeamPhoto.forTeam(ownId, OnlineService.current?.uid);
    if (local != null) {
      return Image.memory(local, fit: fit, cacheWidth: cacheWidth, gaplessPlayback: true);
    }
  }
  if (asset.startsWith('https://')) {
    return Image.network(asset, fit: fit, cacheWidth: cacheWidth, gaplessPlayback: true,
      errorBuilder: (_, __, ___) => Image.asset(kFsCustomFallbackArt, fit: fit, cacheWidth: cacheWidth));
  }
  return Image.asset(asset, fit: fit, cacheWidth: cacheWidth,
    errorBuilder: (_, __, ___) => const SizedBox());
}
