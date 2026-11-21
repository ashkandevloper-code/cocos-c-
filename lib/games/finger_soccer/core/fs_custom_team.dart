// A custom photo team is one cosmetic entitlement per account. Its physics are the
// same as the free Persepolis team (6 power / 7 accuracy); the version in the id
// only busts the public image cache. Never accept a URL supplied by a match peer.
import '../../../core/online/online_config.dart';

const String kFsCustomItem = 'fs_team:custom';
const int kFsCustomPrice = 1000;
const String kFsCustomBucket = 'nx-fs-teams';
const String kFsCustomFallbackArt = 'assets/games/bermuda/discs/persepolis.webp';
const int kFsCustomMaxPngBytes = 1024 * 1024;

// The uid and revision are server-provided (nx_fs_custom_status). An arbitrary
// string / external URL in a setup packet must never become an image request.
final RegExp _customId = RegExp(
    r'^custom:([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}):([1-9][0-9]{9,15})$');

String fsCustomTeamId(String uid, int revision) => 'custom:$uid:$revision';

String? fsCustomOwner(String id) => _customId.firstMatch(id)?.group(1);
int? fsCustomRevision(String id) => int.tryParse(_customId.firstMatch(id)?.group(2) ?? '');

/// This is only a cosmetic URL; it never confers ownership. The server prices
/// the entitlement and permits image uploads only for paid accounts.
String? fsCustomArtUrl(String id) {
  final RegExpMatch? match = _customId.firstMatch(id);
  if (match == null || OnlineConfig.supabaseUrl.isEmpty) return null;
  final Uri base = Uri.parse(OnlineConfig.supabaseUrl);
  if (base.scheme != 'https' || base.host.isEmpty) return null;
  return base.replace(
    path: '/storage/v1/object/public/$kFsCustomBucket/${match.group(1)}/team.png',
    queryParameters: <String, String>{'v': match.group(2)!},
  ).toString();
}

bool fsIsPng(List<int> bytes) => bytes.length >= 8 &&
    bytes[0] == 137 && bytes[1] == 80 && bytes[2] == 78 && bytes[3] == 71 &&
    bytes[4] == 13 && bytes[5] == 10 && bytes[6] == 26 && bytes[7] == 10;
