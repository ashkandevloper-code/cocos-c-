import 'package:flame/game.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/games/arcade_kit/arcade_loop.dart';

class _PhysicsPage extends StatefulWidget {
  const _PhysicsPage();
  @override
  State<_PhysicsPage> createState() => _PhysicsPageState();
}

class _PhysicsPageState extends ArcadeLoopState<_PhysicsPage> {
  double traveled = 0;
  @override
  void tick(double dt) => traveled += dt * 10;
  @override
  Widget build(BuildContext context) => SizedBox(
    width: 200, height: 300,
    child: flameCanvas((Canvas canvas, Size size) {
      canvas.drawCircle(Offset(traveled, size.height / 2), 5, Paint()..color = Colors.white);
    }),
  );
}

void main() {
  testWidgets('Flame owns the physical frame clock and board rendering', (WidgetTester tester) async {
    await tester.pumpWidget(const MaterialApp(home: Scaffold(body: _PhysicsPage())));
    await tester.pump(const Duration(milliseconds: 100));
    await tester.pump(const Duration(milliseconds: 100));
    expect(find.byType(GameWidget<ArcadeFlameGame>), findsOneWidget);
    final _PhysicsPageState state = tester.state(find.byType(_PhysicsPage));
    expect(state.traveled, greaterThan(0));
    await tester.pumpWidget(const SizedBox.shrink());
  });
}
