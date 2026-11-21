import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import '../../games/finger_soccer/core/bm_data.dart';
import '../../games/finger_soccer/core/fs_custom_team.dart';
import '../../games/finger_soccer/core/fs_custom_team_photo.dart';
import '../online/online_service.dart';
import '../profile/profile_service.dart';

/// Finger-soccer teams that are always unlocked (no purchase): Persepolis, Esteghlal and Iran.
const Set<String> kFreeTeams = <String>{'club32', 'club33', 'nat26'};

/// Default team for a new player / when a saved team is not owned.
const String kDefaultTeam = 'club32';

/// Inventory / catalog id of a finger-soccer team (`fs_team:club0`).
/// The revision-bearing custom team ID always maps to ONE 1000-gem entitlement.
String teamItemId(String teamId) => teamId == 'custom' || fsCustomOwner(teamId) != null
    ? kFsCustomItem
    : 'fs_team:$teamId';

/// Built-in gem price of a team (the server catalog, editable by admins, wins when loaded). Free teams cost 0; every
/// other team costs 150..500 gems by strength (power + accuracy 9..20, rounded to 10): weakest 150, luxury 10/10 500.
/// supabase/migrations/20261120000000_nx_gems_shop.sql seeds nx_catalog with exactly these numbers.
int defaultTeamPrice(String teamId) {
  if (teamItemId(teamId) == kFsCustomItem) return kFsCustomPrice;
  if (kFreeTeams.contains(teamId)) return 0;
  BmTeamDef? d;
  for (final BmTeamDef t in kBmTeamDefs) {
    if (t.id == teamId) d = t;
  }
  if (d == null) return 500;
  final int score = (d.power + d.accuracy).clamp(9, 20);
  return ((150 + (score - 9) * 350 / 11) / 10).round() * 10;
}

/// Result of a gem purchase.
class GemBuyResult {
  const GemBuyResult(this.ok, {this.error, this.gems});
  final bool ok;
  final String? error; // insufficient_gems | offline | not_for_sale | not_signed_in | ...
  final int? gems;
}

/// Gems (الماس) and the gem shop. Balance and ownership are server-side (auth project, public.profiles.gems and
/// nx_profiles.data.inv); every purchase is an RPC that checks the catalog price and deducts atomically.
class GemShop extends ChangeNotifier {
  GemShop._();
  static final GemShop instance = GemShop._();

  /// Live gem prices from nx_catalog (item id -> price); empty until loaded.
  final Map<String, int> _prices = <String, int>{};
  final Set<String> _inactive = <String>{};
  bool loaded = false;
  bool busy = false;
  String? dailyClaimedOn;

  // Unlike older teams, this entitlement is never inferred from the locally
  // cached profile. It is read from the auth project's protected inventory RPC.
  String? _customUid;
  bool _customOwned = false;
  int _customVersion = 0;
  int _customCheck = 0;

  SupabaseClient? get _c {
    final OnlineService? o = OnlineService.current;
    final SupabaseClient? c = o?.client;
    if (c == null || c.auth.currentSession == null) return null;
    return c;
  }

  ProfileService? get _p => ProfileService.current;

  bool get online => _c != null;

  int teamPrice(String teamId) => teamItemId(teamId) == kFsCustomItem
      ? kFsCustomPrice
      : kFreeTeams.contains(teamId) ? 0 : (_prices[teamItemId(teamId)] ?? defaultTeamPrice(teamId));

  bool teamForSale(String teamId) => !_inactive.contains(teamItemId(teamId));

  String? get _signedInUid => _c?.auth.currentUser?.id;

  /// The purchase can succeed before a photo has been uploaded. Only an actual
  /// server-confirmed photo makes the team fieldable.
  bool get customEntitled => _signedInUid != null && _signedInUid == _customUid && _customOwned;
  String? get customTeamId => customEntitled && _customVersion > 0
      ? fsCustomTeamId(_customUid!, _customVersion)
      : null;

  bool ownsTeam(String teamId) {
    if (fsCustomOwner(teamId) != null) return teamId == customTeamId;
    if (kFreeTeams.contains(teamId) || teamPrice(teamId) == 0) return true;
    return _p?.owns(teamItemId(teamId)) ?? false;
  }

  /// A team the player may field. An old photo revision is upgraded to the
  /// latest server-confirmed one; another account's ID is never accepted.
  String usableTeam(String teamId) {
    final String? owner = fsCustomOwner(teamId);
    if (owner != null) return owner == _signedInUid ? (customTeamId ?? kDefaultTeam) : kDefaultTeam;
    return ownsTeam(teamId) ? teamId : kDefaultTeam;
  }

  void _applyWallet(dynamic r) {
    if (r is! Map) return;
    _p?.applyWallet(
        xp: (r['xp'] as num?)?.toInt(), coins: (r['coins'] as num?)?.toInt(), gems: (r['gems'] as num?)?.toInt());
  }

  /// Re-read ownership + photo revision from the main project's server. The
  /// version only changes when the object really exists in Storage; local
  /// inventory and file paths are not proof of payment.
  Future<int?> refreshCustom() async {
    final int check = ++_customCheck;
    final SupabaseClient? c = _c;
    final String? uid = c?.auth.currentUser?.id;
    if (c == null || uid == null) {
      _customUid = null;
      _customOwned = false;
      _customVersion = 0;
      FsCustomTeamPhoto.clear();
      notifyListeners();
      return null;
    }
    if (_customUid != uid) {
      _customOwned = false;
      _customVersion = 0;
      FsCustomTeamPhoto.clear();
    }
    _customUid = uid;
    try {
      final dynamic result = await c.rpc('nx_fs_custom_status');
      if (check != _customCheck || _signedInUid != uid) return null;
      final Map<dynamic, dynamic> r = result is Map ? result : const <dynamic, dynamic>{};
      _customOwned = r['ok'] == true && r['owned'] == true;
      final int revision = (r['version'] as num?)?.toInt() ?? 0;
      _customVersion = _customOwned && r['photo'] == true &&
              fsCustomOwner(fsCustomTeamId(uid, revision)) == uid
          ? revision
          : 0;
      if (_customVersion > 0) {
        await FsCustomTeamPhoto.load(uid, _customVersion);
      } else {
        FsCustomTeamPhoto.clear();
      }
    } catch (e) {
      if (check != _customCheck || _signedInUid != uid) return null;
      debugPrint('nx_fs_custom_status: $e');
      _customOwned = false;
      _customVersion = 0;
      FsCustomTeamPhoto.clear();
    }
    if (check != _customCheck || _signedInUid != uid) return null;
    notifyListeners();
    return _customVersion;
  }

  /// Wallet (with the one-time welcome gems) + catalog gem prices.
  Future<void> refresh() async {
    final SupabaseClient? c = _c;
    if (c == null) {
      await refreshCustom();
      return;
    }
    try {
      _applyWallet(await c.rpc('nx_wallet'));
    } catch (e) {
      debugPrint('nx_wallet: $e');
    }
    try {
      final List<dynamic> rows = await c.from('nx_catalog').select('item_id, price, currency, active').eq('currency', 'gems');
      _prices.clear();
      _inactive.clear();
      for (final dynamic e in rows) {
        final Map<String, dynamic> m = Map<String, dynamic>.from(e as Map);
        _prices['${m['item_id']}'] = (m['price'] as num?)?.toInt() ?? 0;
        if (m['active'] == false) _inactive.add('${m['item_id']}');
      }
      loaded = true;
    } catch (e) {
      debugPrint('nx_catalog: $e');
    }
    await refreshCustom();
    notifyListeners();
  }

  /// Buys a finger-soccer team with gems (server checks the price, the balance and ownership).
  Future<GemBuyResult> buyTeam(String teamId) => buy(teamItemId(teamId));

  /// Upload a canonical 256x256 PNG after paying. The separate bucket's RLS
  /// independently checks the protected inventory and exact object path.
  Future<void> uploadCustomPhoto(Uint8List png) async {
    final SupabaseClient? c = _c;
    final String? uid = _signedInUid;
    if (c == null || uid == null || !customEntitled) throw StateError('purchase_required');
    if (busy) throw StateError('busy');
    if (!fsIsPng(png) || png.length > kFsCustomMaxPngBytes) throw const FormatException('invalid_photo');
    busy = true;
    notifyListeners();
    try {
      await c.storage.from(kFsCustomBucket).uploadBinary(
        '$uid/team.png', png,
        fileOptions: const FileOptions(contentType: 'image/png', upsert: true),
      );
      if (_signedInUid != uid) throw StateError('account_changed');
      final int? verifiedRevision = await refreshCustom();
      if (_signedInUid != uid || verifiedRevision == null || verifiedRevision <= 0 ||
          customTeamId != fsCustomTeamId(uid, verifiedRevision)) {
        throw StateError('photo_not_verified');
      }
      await FsCustomTeamPhoto.save(uid, verifiedRevision, png);
      notifyListeners();
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  Future<GemBuyResult> buy(String itemId) async {
    final SupabaseClient? c = _c;
    final String? buyer = c?.auth.currentUser?.id;
    if (c == null || buyer == null) return const GemBuyResult(false, error: 'offline');
    if (busy) return const GemBuyResult(false, error: 'busy');
    busy = true;
    notifyListeners();
    try {
      final dynamic r = await c.rpc('nx_buy_with_gems', params: <String, dynamic>{'p_item': itemId});
      if (_signedInUid != buyer) return const GemBuyResult(false, error: 'account_changed');
      if (r is Map) {
        _applyWallet(r);
        if (r['ok'] == true) {
          _p?.grantItem(itemId);
          if (itemId == kFsCustomItem) await refreshCustom();
          return GemBuyResult(true, gems: (r['gems'] as num?)?.toInt());
        }
        return GemBuyResult(false, error: '${r['error'] ?? 'failed'}', gems: (r['gems'] as num?)?.toInt());
      }
      return const GemBuyResult(false, error: 'failed');
    } on PostgrestException catch (e) {
      return GemBuyResult(false, error: e.message);
    } catch (e) {
      return const GemBuyResult(false, error: 'network');
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  /// Daily free gems (once per day, server-checked). Returns the gems granted (0 = already claimed / offline).
  Future<int> claimDaily() async {
    final SupabaseClient? c = _c;
    if (c == null) return 0;
    try {
      final dynamic r = await c.rpc('nx_daily_gems');
      _applyWallet(r);
      notifyListeners();
      return r is Map ? ((r['granted'] as num?)?.toInt() ?? 0) : 0;
    } catch (e) {
      debugPrint('nx_daily_gems: $e');
      return 0;
    }
  }
}

/// Persian / English text for a gem-shop error.
String gemErrorText(String? e, bool fa) => switch (e) {
      'insufficient_gems' => fa ? 'الماس کافی نداری' : 'Not enough gems',
      'offline' || 'not_signed_in' => fa ? 'برای خرید با الماس وارد حساب شو (آنلاین)' : 'Sign in (online) to buy with gems',
      'not_for_sale' => fa ? 'این آیتم فعلاً فروشی نیست' : 'Not for sale right now',
      'network' => fa ? 'اتصال برقرار نیست' : 'No connection',
      'banned' => fa ? 'حساب مسدود است' : 'Account banned',
      _ => fa ? 'خرید انجام نشد' : 'Purchase failed',
    };
