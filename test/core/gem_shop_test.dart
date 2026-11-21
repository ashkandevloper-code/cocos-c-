import 'dart:ui' as ui;

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/economy/gem_shop.dart';
import 'package:nexora/games/finger_soccer/core/bm_data.dart';
import 'package:nexora/games/finger_soccer/intro/fs_intro_data.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('team gem prices: three free teams, every other team 150..500, stronger teams cost more', () {
    expect(kFreeTeams.length, 3);
    expect(kFreeTeams.contains(kDefaultTeam), isTrue);
    for (final BmTeamDef t in kBmTeamDefs) {
      final int p = defaultTeamPrice(t.id);
      if (kFreeTeams.contains(t.id)) {
        expect(p, 0, reason: t.id);
      } else {
        expect(p, inInclusiveRange(150, 500), reason: t.id);
        expect(p % 10, 0, reason: t.id);
      }
    }
    expect(defaultTeamPrice('lux0'), 500);
    expect(defaultTeamPrice('club0'), greaterThan(defaultTeamPrice('club38')));
    expect(teamItemId('club0'), 'fs_team:club0');
  });

  test('new car teams append without changing saved roster indices; art decodes and price matches catalog', () async {
    expect(kBmTeamDefs[113].id, 'barca_gold');
    expect(kBmTeamDefs.length, 118);
    final Map<String, String> art = <String, String>{
      'car14': 'audi', 'car15': 'volkswagen', 'car16': 'toyota', 'car17': 'nissan',
    };
    for (final MapEntry<String, String> entry in art.entries) {
      final BmTeamDef team = kBmTeamDefs.singleWhere((BmTeamDef t) => t.id == entry.key);
      expect(team.cat, 'lux');
      expect(team.art, 'teams/${entry.value}');
      expect(team.power, 10);
      expect(team.accuracy, 10);
      expect(kFsTeamFa.containsKey(team.name), isTrue);
      expect(defaultTeamPrice(team.id), 500);
      expect(teamItemId(team.id), 'fs_team:${team.id}');
      final ByteData bytes = await rootBundle.load('assets/games/bermuda/${team.art}.webp');
      final ui.Codec codec = await ui.instantiateImageCodec(bytes.buffer.asUint8List());
      final ui.FrameInfo frame = await codec.getNextFrame();
      expect(frame.image.width, 256);
      expect(frame.image.height, 256);
      frame.image.dispose();
      codec.dispose();
    }
  });

  test('locked teams fall back to the default free team when signed out', () {
    expect(GemShop.instance.ownsTeam('club32'), isTrue);
    expect(GemShop.instance.ownsTeam('lux0'), isFalse);
    expect(GemShop.instance.usableTeam('lux0'), kDefaultTeam);
  });
}
