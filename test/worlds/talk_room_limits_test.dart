import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/worlds/rooms/talk_room_page.dart';

void main() {
  test(
    'active seat limits follow game metadata, independently of room members',
    () {
      final dynamic s = _S();
      expect(s.max('mafia'), 12);
      expect(s.min('mafia'), 6);
      expect(s.max('spy_mafia'), 8);
      expect(s.min('spy_mafia'), 4);
      expect(s.max('finger_soccer'), 6);
      expect(s.max('carrom'), 2);
      expect(s.max('quiz'), 2);
    },
  );
}

class _S {
  int max(String g) => TalkRoomLimits.maxPlayersFor(g);
  int min(String g) => TalkRoomLimits.minPlayersFor(g);
}
