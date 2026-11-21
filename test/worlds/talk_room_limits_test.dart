import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/games/game_registry.dart';
import 'package:nexora/worlds/rooms/talk_room_page.dart';

void main() {
  test('talk-room active seats mirror the executable game registry', () {
    expect(TalkRoomLimits.maxPlayersFor('finger_soccer'), 6); // 3v3
    expect(TalkRoomLimits.minPlayersFor('finger_soccer'), 2);
    expect(TalkRoomLimits.maxPlayersFor('mafia'), 12);
    expect(TalkRoomLimits.minPlayersFor('mafia'), 6);
    expect(TalkRoomLimits.maxPlayersFor('spy_mafia'), 8);
    expect(TalkRoomLimits.minPlayersFor('spy_mafia'), 4);
    expect(TalkRoomLimits.maxPlayersFor('quiz'), 2);
    expect(TalkRoomLimits.maxPlayersFor('carrom'), 2);
    for (final GameInfo g in kGames) {
      expect(TalkRoomLimits.minPlayersFor(g.id), g.minPlayers, reason: g.id);
      expect(TalkRoomLimits.maxPlayersFor(g.id),
          g.id == 'finger_soccer' ? 6 : g.maxPlayers,
          reason: g.id);
    }
    expect(TalkRoomLimits.maxPlayersFor('chess_blitz'), 0);
  });
}
