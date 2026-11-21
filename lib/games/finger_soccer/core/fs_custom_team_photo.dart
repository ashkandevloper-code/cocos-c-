import 'dart:io';
import 'dart:typed_data';

import 'package:path_provider/path_provider.dart';

import 'fs_custom_team.dart';

/// Local copy of this account's purchased photo so a bot/local match can draw
/// the real custom discs when the network drops after entitlement sync. The
/// revision keeps another device's newer upload from showing an old local PNG.
class FsCustomTeamPhoto {
  FsCustomTeamPhoto._();

  static String? _uid;
  static int? _revision;
  static Uint8List? _bytes;

  static Uint8List? forTeam(String teamId, String? signedInUid) =>
      signedInUid != null && signedInUid == _uid &&
      fsCustomOwner(teamId) == signedInUid && fsCustomRevision(teamId) == _revision
          ? _bytes : null;

  static Future<File> _file(String uid, String ext) async {
    final Directory base = await getApplicationSupportDirectory();
    return File('${base.path}/fs_custom_team_$uid.$ext');
  }

  static Future<void> load(String uid, int revision) async {
    if (_uid == uid && _revision == revision && _bytes != null) return;
    _uid = uid;
    _revision = revision;
    _bytes = null;
    try {
      final File versionFile = await _file(uid, 'rev');
      if (await versionFile.readAsString() != '$revision') return;
      final File file = await _file(uid, 'png');
      if (await file.exists() && await file.length() <= kFsCustomMaxPngBytes) {
        final Uint8List bytes = await file.readAsBytes();
        if (_uid == uid && _revision == revision && fsIsPng(bytes)) _bytes = bytes;
      }
    } catch (_) {
      // The public URL remains usable; failure to cache must not undo a purchase.
    }
  }

  static Future<void> save(String uid, int revision, Uint8List bytes) async {
    if (fsCustomOwner(fsCustomTeamId(uid, revision)) != uid ||
        !fsIsPng(bytes) || bytes.length > kFsCustomMaxPngBytes) {
      throw const FormatException('invalid_photo');
    }
    _uid = uid;
    _revision = revision;
    _bytes = bytes;
    try {
      await (await _file(uid, 'png')).writeAsBytes(bytes, flush: true);
      // Commit the revision last: an interrupted write must not claim that an
      // old photo matches the latest object on the server.
      await (await _file(uid, 'rev')).writeAsString('$revision', flush: true);
    } catch (_) {
      // Keep the in-memory copy for this session even when disk storage fails.
    }
  }

  static void clear() {
    _uid = null;
    _revision = null;
    _bytes = null;
  }
}
