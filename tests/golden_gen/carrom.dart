import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/carrom/core/carrom_match.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/carrom/core/carrom_bot.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/carrom/core/carrom_physics.dart';
String f(double v) => v.toStringAsFixed(6);
void main() {
  for (final cfg in [[CrBotLevel.hard, CrBotLevel.normal, 3], [CrBotLevel.easy, CrBotLevel.hard, 8]]) {
    final m = CrMatch.newBoard();
    final bots = [CrBot(seat: 0, level: cfg[0] as CrBotLevel, seed: cfg[2] as int), CrBot(seat: 1, level: cfg[1] as CrBotLevel, seed: (cfg[2] as int) + 1)];
    int shots = 0;
    while (m.phase != CrPhase.finished && shots < 40) {
      final b = bots[m.turn];
      var s = b.choose(m);
      s = b.withExecutionNoise(s);
      final t = m.turn;
      final o = m.playShot(s);
      print('cr ${cfg[2]} #$shots t$t place${f(s.placeX)} dir${f(s.dirX)},${f(s.dirY)} pw${f(s.power)} st${o.steps} own${o.ownPocketed} opp${o.oppPocketed} q${o.queenPocketed} foul${o.foul} cov${o.queenCovered} ret${o.returnedIds} cont${o.continueTurn} h${m.stateHash()}');
      shots++;
    }
    print('cr ${cfg[2]} end winner ${m.winner} pts ${m.points} rem ${m.remaining(0)}/${m.remaining(1)} json ${m.toJson()['queenCoveredBy']}');
  }
  final m2 = CrMatch.newBoard();
  m2.passTurn();
  print('pass ${m2.turn} ${m2.turnNumber} ${crStateHash(crInitialBodies())}');
}
