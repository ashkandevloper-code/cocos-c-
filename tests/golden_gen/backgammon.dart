import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/backgammon/core/backgammon_rules.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/backgammon/core/backgammon_bot.dart';

void main() {
  for (final seed in [5, 77]) {
    final dice = BgDice(seed);
    final bots = [BgBot(level: seed == 5 ? 2 : 0, seed: seed), BgBot(level: 1, seed: seed + 1)];
    var s = BgState.initial();
    int p = 0;
    int turns = 0;
    while (s.winner == null && turns < 400) {
      final d = [dice.roll(), dice.roll()];
      final plays = BgRules.enumeratePlays(s, p, d);
      final moves = bots[p].choose(s, p, d);
      for (final m in moves) {
        s = BgRules.apply(s, p, m);
      }
      print('bg $seed t$turns p$p d${d.join(",")} plays${plays.length} ${moves.join(" ")} ev${BgBot.evaluate(s, p).toStringAsFixed(4)} pip${s.pip(0)}/${s.pip(1)}');
      p = 1 - p;
      turns++;
    }
    print('bg $seed winner ${s.winner} mult ${s.winner == null ? 0 : BgRules.resultMultiplier(s, s.winner!)} key ${s.key}');
  }
  // TurnTracker forced-die rule + undo.
  final st = BgState.initial();
  final tt = BgTurnTracker(st, 0, [6, 5]);
  print('tt total ${tt.total} force ${tt.forceDie} legal ${tt.legalMoves().length}');
  final first = tt.legalMoves().first;
  tt.play(first);
  print('tt after ${tt.played} remaining ${tt.remaining} legal ${tt.legalMoves().length}');
  tt.undo();
  print('tt undo ${tt.played.length} ${tt.state.key}');
}
