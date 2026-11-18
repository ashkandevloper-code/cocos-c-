// Golden trace generator. Run: dart snl_ludo.dart > ../golden/snl_ludo.txt  (imports the Flutter repo's pure-Dart cores)
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/snakes_ladders/core/snl_rules.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/ludo/core/ludo_rules.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/ludo/core/ludo_bot.dart';

void main() {
  for (final seed in [1, 7, 2024]) {
    final e = SnlEngine(playerCount: 3, seed: seed);
    int n = 0;
    while (e.winner == null && n < 500) {
      final r = e.move(e.rollDie());
      print('snl $seed s${r.seat} r${r.roll} ${r.from}>${r.landed}>${r.finalPos} ${r.steps.join(",")} w${r.won} x${r.extraTurn}');
      n++;
    }
  }
  for (final seed in [3, 11]) {
    for (final pc in [2, 4]) {
      final e = LudoEngine(playerCount: pc, seed: seed);
      final bots = [for (int i = 0; i < pc; i++) LudoBot(level: i % 3, seed: seed * 10 + i)];
      int n = 0;
      while (e.state.winner == null && n < 3000) {
        final seat = e.state.turn;
        final rr = e.roll();
        var line = 'ludo $seed/$pc s$seat r${rr.roll} m${rr.moves.length} f${rr.forfeited} a${rr.rollAgain}';
        if (rr.moves.isNotEmpty) {
          final m = bots[seat].choose(e.state, rr.moves);
          final p = e.play(m);
          line += ' $m e${p.extraTurn} w${p.won} sc${LudoBot.score(e.state, m).toStringAsFixed(3)}';
        }
        print(line);
        n++;
      }
      print('ludo $seed/$pc winner ${e.state.winner} tokens ${e.state.tokens}');
    }
  }
}
