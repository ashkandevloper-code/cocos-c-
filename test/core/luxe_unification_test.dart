import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/theme/nx_theme.dart';
import 'package:nexora/core/ui2/anix_kit.dart';
import 'package:nexora/core/ui2/luxe.dart';
import 'package:nexora/core/ui2/nx2_widgets.dart';

void main() {
  test('shared app chrome uses the Home Page palette, not the old purple skin', () {
    expect(Nx2.deep, Luxe.bg);
    expect(Nx2.gold, Luxe.gold);
    expect(Nx2.text, Luxe.ink);
    expect(buildNxTheme().extension<NxThemeColors>()!.background, Luxe.bg);
  });

  testWidgets('world backgrounds, cards and actions use Station Luxe', (WidgetTester tester) async {
    int taps = 0;
    await tester.pumpWidget(MaterialApp(
      theme: buildNxTheme(),
      home: Scaffold(body: Nx2Background(scene: Nx2Scene.arcade,
        child: Center(child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
          const Nx2Panel(child: Text('Card')),
          const SizedBox(height: 12),
          Nx2Button(label: 'Continue', onTap: () => taps++),
        ])),
      )),
    ));
    await tester.pump(const Duration(milliseconds: 80));
    expect(find.byType(LuxeScene), findsOneWidget);
    expect(find.byType(LuxeDust), findsNothing, reason: 'secondary screens have no extra animation ticker');
    expect(find.byType(LuxeSurface), findsOneWidget);
    expect(find.byType(AnixButton), findsOneWidget);
    await tester.tap(find.text('Continue'));
    expect(taps, 1);
    await tester.pumpWidget(const SizedBox.shrink());
  });

  testWidgets('shared dialog is a readable black/gold surface', (WidgetTester tester) async {
    await tester.pumpWidget(MaterialApp(
      theme: buildNxTheme(),
      home: const Scaffold(body: Center(child: AnixDialog(
        title: Text('Review'), content: Text('Details'),
      ))),
    ));
    expect(find.byType(LuxeSurface), findsOneWidget);
    expect(find.text('Review'), findsOneWidget);
    expect(find.text('Details'), findsOneWidget);
  });
}
