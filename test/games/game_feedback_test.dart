import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/games/core/game_contract.dart';

GameLaunch launch({required bool sound, required bool haptics}) => GameLaunch(
      gameId: 'uno',
      mode: GameMode.vsBot,
      difficulty: BotDifficulty.normal,
      playerName: 'Tester',
      isFa: false,
      fancyEffects: false,
      reducedMotion: false,
      soundOn: sound,
      hapticsOn: haptics,
      onFinish: (_) {},
    );

void main() {
  testWidgets('game sound and haptics are independent preferences', (WidgetTester tester) async {
    final List<MethodCall> calls = <MethodCall>[];
    final messenger = TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    messenger.setMockMethodCallHandler(SystemChannels.platform, (MethodCall call) async {
      calls.add(call);
      return null;
    });
    addTearDown(() => messenger.setMockMethodCallHandler(SystemChannels.platform, null));

    final GameLaunch silentHaptics = launch(sound: true, haptics: false);
    expect(silentHaptics.soundOn, isTrue);
    silentHaptics.hapticSelection();
    silentHaptics.hapticLight();
    silentHaptics.hapticMedium();
    silentHaptics.hapticHeavy();
    await tester.pump();
    expect(calls, isEmpty);

    final GameLaunch silentAudio = launch(sound: false, haptics: true);
    expect(silentAudio.soundOn, isFalse);
    silentAudio.hapticSelection();
    silentAudio.hapticLight();
    silentAudio.hapticMedium();
    silentAudio.hapticHeavy();
    await tester.pump();
    expect(calls.where((MethodCall c) => c.method == 'HapticFeedback.vibrate').length, 4);
  });
}
