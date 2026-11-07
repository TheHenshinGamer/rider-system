// Minimal dependency-free test runner for the engine-free core.
#include "core/Config.h"
#include "core/EventBus.h"
#include "core/Keys.h"
#include "core/LayerRegistry.h"
#include "core/Log.h"
#include "input/InputLayer.h"

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	int g_failures = 0;
	int g_checks = 0;

	void Check(bool a_ok, const char* a_expr, const char* a_file, int a_line)
	{
		++g_checks;
		if (!a_ok) {
			++g_failures;
			std::fprintf(stderr, "  FAIL %s:%d: %s\n", a_file, a_line, a_expr);
		}
	}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

	struct TestCase
	{
		const char* name;
		std::function<void()> fn;
	};

	std::vector<TestCase>& Tests()
	{
		static std::vector<TestCase> tests;
		return tests;
	}

	struct Registrar
	{
		Registrar(const char* a_name, std::function<void()> a_fn) { Tests().push_back({ a_name, std::move(a_fn) }); }
	};

#define TEST(name)                                       \
	void name();                                         \
	const Registrar name##_registrar{ #name, &name };    \
	void name()

	using namespace rcats;
	using input::Action;
	using input::ActionEvent;
	using input::Direction;

	constexpr std::uint32_t W = 0x11, S = 0x1F, A = 0x1E, D = 0x20, LMB = 0x100;

	// ---- Keys ---------------------------------------------------------------

	TEST(KeysParseNamesAndNumbers)
	{
		CHECK(keys::Parse("W") == 0x11u);
		CHECK(keys::Parse("w") == 0x11u);
		CHECK(keys::Parse(" Mouse1 ") == 0x100u);
		CHECK(keys::Parse("LMB") == 0x100u);
		CHECK(keys::Parse("PadRT") == keys::kGamepadRightTrigger);
		CHECK(keys::Parse("0x2A") == 0x2Au);
		CHECK(keys::Parse("57") == 57u);
		CHECK(!keys::Parse("NotAKey"));
		CHECK(!keys::Parse("0x999"));
		CHECK(keys::Name(0x100) == "Mouse1");
		CHECK(keys::Name(0x77) == "0x77");
	}

	TEST(KeysGamepadIdCodes)
	{
		CHECK(keys::FromGamepadIdCode(0x1000) == keys::kGamepadA);
		CHECK(keys::FromGamepadIdCode(0x0001) == keys::kGamepadDPadUp);
		CHECK(keys::FromGamepadIdCode(0x0A) == keys::kGamepadRightTrigger);
		CHECK(!keys::FromGamepadIdCode(0x0400));
	}

	TEST(KeysVirtualKeyToScanCode)
	{
		CHECK(keys::FromVirtualKey(0x57) == W);       // 'W'
		CHECK(keys::FromVirtualKey(0x41) == A);       // 'A'
		CHECK(keys::FromVirtualKey(0x53) == S);       // 'S'
		CHECK(keys::FromVirtualKey(0x44) == D);       // 'D'
		CHECK(keys::FromVirtualKey(0x26) == 0xC8u);   // VK_UP -> DIK_UP (the key seen in-game)
		CHECK(keys::FromVirtualKey(0xA1) == 0x36u);   // VK_RSHIFT -> DIK_RSHIFT
		CHECK(keys::FromVirtualKey(0x20) == 0x39u);   // Space
		CHECK(keys::FromVirtualKey(0x7B) == 0x58u);   // F12
		CHECK(!keys::FromVirtualKey(0xFF));
	}

	// ---- Config -------------------------------------------------------------

	TEST(ConfigDefaultsWhenEmpty)
	{
		const auto result = ParseConfig("");
		CHECK(result.warnings.empty());
		CHECK(result.config.IsEnabled(LayerId::kInput));
		CHECK(result.config.input.forward == KeyList{ W });
	}

	TEST(ConfigParsesSections)
	{
		const auto result = ParseConfig(
			"; comment\r\n"
			"[General]\n"
			"LogLevel = debug\n"
			"[Layers]\n"
			"Combo = 0   # inline comment\n"
			"parkour=false\n"
			"[Input]\n"
			"Attack = Mouse1, PadRB\n"
			"PowerAttack = none\n"
			"PowerAttackHoldMs = 450\n"
			"TriggerThreshold = 0.7\n"
			"LogRawInput = 1\n");
		CHECK(result.warnings.empty());
		CHECK(result.config.logLevel == log::Level::kDebug);
		CHECK(!result.config.IsEnabled(LayerId::kCombo));
		CHECK(!result.config.IsEnabled(LayerId::kParkour));
		CHECK(result.config.IsEnabled(LayerId::kInput));
		CHECK((result.config.input.attack == KeyList{ 0x100, keys::kGamepadRightShoulder }));
		CHECK(result.config.input.powerAttack.empty());
		CHECK(result.config.input.powerAttackHoldMs == 450.0f);
		CHECK(result.config.input.triggerThreshold == 0.7f);
		CHECK(result.config.input.logRawInput);
	}

	TEST(ConfigBadValuesWarnAndKeepDefaults)
	{
		const auto result = ParseConfig(
			"[General]\nLogLevel = loud\n"
			"[Input]\nForward = Banana\nChordWindowMs = -5\nMystery = 1\n"
			"[Layers\n");
		CHECK(result.warnings.size() == 5);
		CHECK(result.config.logLevel == log::Level::kInfo);
		CHECK(result.config.input.forward == KeyList{ W });
		CHECK(result.config.input.chordWindowMs == 120.0f);
	}

	TEST(ConfigMissingFile)
	{
		const auto result = LoadConfig("definitely/not/here.ini");
		CHECK(!result.fileFound);
		CHECK(result.warnings.size() == 1);
	}

	// ---- EventBus -----------------------------------------------------------

	struct Ping { int value; };
	struct Pong { int value; };

	TEST(EventBusRoutesByType)
	{
		EventBus bus;
		int pings = 0, pongs = 0;
		const auto token = bus.Subscribe<Ping>([&](const Ping& p) { pings += p.value; });
		bus.Subscribe<Pong>([&](const Pong& p) { pongs += p.value; });

		bus.Publish(Ping{ 2 });
		bus.Publish(Pong{ 5 });
		CHECK(pings == 2);
		CHECK(pongs == 5);

		bus.Unsubscribe(token);
		bus.Publish(Ping{ 2 });
		CHECK(pings == 2);
		CHECK(bus.SubscriberCount() == 1);
	}

	TEST(EventBusIsolatesThrowingHandlers)
	{
		EventBus bus;
		int reached = 0;
		bus.Subscribe<Ping>([](const Ping&) { throw std::runtime_error("boom"); });
		bus.Subscribe<Ping>([&](const Ping&) { ++reached; });
		bus.Publish(Ping{ 0 });
		CHECK(reached == 1);
	}

	// ---- LayerRegistry ------------------------------------------------------

	TEST(LayerRegistryFailSafe)
	{
		Config cfg;
		cfg.layers[static_cast<std::size_t>(LayerId::kCombo)] = false;

		LayerRegistry reg;
		reg.Register(LayerId::kInput, [] { return true; });
		reg.Register(LayerId::kCombo, [] { return true; });
		reg.Register(LayerId::kHitDetect, [] { return false; });
		reg.Register(LayerId::kImpact, []() -> bool { throw std::runtime_error("address not found"); });
		reg.InstallAll(cfg);

		CHECK(reg.GetState(LayerId::kInput) == LayerState::kActive);
		CHECK(reg.GetState(LayerId::kCombo) == LayerState::kDisabledByConfig);
		CHECK(reg.GetState(LayerId::kHitDetect) == LayerState::kFailed);
		CHECK(reg.GetState(LayerId::kImpact) == LayerState::kFailed);
		CHECK(reg.GetState(LayerId::kParkour) == LayerState::kNotRegistered);
	}

	// ---- InputLayer ---------------------------------------------------------

	struct InputFixture
	{
		EventBus bus;
		std::vector<ActionEvent> events;
		std::vector<input::BlockEvent> blocks;
		input::InputLayer layer;

		explicit InputFixture(InputConfig a_cfg = {}) :
			layer(std::move(a_cfg), bus)
		{
			bus.Subscribe<ActionEvent>([this](const ActionEvent& e) { events.push_back(e); });
			bus.Subscribe<input::BlockEvent>([this](const input::BlockEvent& e) { blocks.push_back(e); });
		}
	};

	TEST(InputTapIsLightAttack)
	{
		InputFixture f;
		f.layer.OnButton(LMB, true, 1.00);
		f.layer.OnButton(LMB, false, 1.10);
		CHECK(f.events.size() == 1);
		CHECK(f.events[0].action == Action::kLightAttack);
		CHECK(f.events[0].direction == Direction::kNeutral);
	}

	TEST(InputHoldIsPowerAttackOnce)
	{
		InputFixture f;
		f.layer.OnButton(LMB, true, 1.00);
		f.layer.OnButton(LMB, true, 1.20);  // held repeat, below threshold
		CHECK(f.events.empty());
		f.layer.Update(1.31);               // crosses 300 ms
		f.layer.OnButton(LMB, true, 1.40);
		f.layer.OnButton(LMB, false, 1.50); // release swallowed
		CHECK(f.events.size() == 1);
		CHECK(f.events[0].action == Action::kPowerAttack);
	}

	TEST(InputDirectionalChords)
	{
		InputFixture f;
		const std::pair<std::uint32_t, Direction> cases[] = {
			{ W, Direction::kForward }, { S, Direction::kBack },
			{ A, Direction::kLeft }, { D, Direction::kRight },
		};
		double t = 0.0;
		for (const auto& [key, dir] : cases) {
			t += 10.0;
			f.layer.OnButton(key, true, t);
			f.layer.OnButton(LMB, true, t + 0.01);
			f.layer.Update(t + 0.40);
			f.layer.OnButton(LMB, false, t + 0.45);
			f.layer.OnButton(key, false, t + 0.50);
		}
		CHECK(f.events.size() == 4);
		for (std::size_t i = 0; i < f.events.size() && i < 4; ++i) {
			CHECK(f.events[i].action == Action::kPowerAttack);
			CHECK(f.events[i].direction == cases[i].second);
		}
	}

	TEST(InputMostRecentDirectionWins)
	{
		InputFixture f;
		f.layer.OnButton(W, true, 1.0);
		f.layer.OnButton(D, true, 1.2);
		CHECK(f.layer.CurrentDirection(1.3) == Direction::kRight);
		f.layer.OnButton(D, false, 1.4);
		CHECK(f.layer.CurrentDirection(1.5) == Direction::kForward);
	}

	TEST(InputChordWindowLeniency)
	{
		InputFixture f;
		f.layer.OnButton(S, true, 1.00);
		f.layer.OnButton(S, false, 1.05);
		CHECK(f.layer.CurrentDirection(1.10) == Direction::kBack);   // 50 ms after release
		CHECK(f.layer.CurrentDirection(1.30) == Direction::kNeutral); // 250 ms, outside 120 ms
	}

	TEST(InputLeftStickDirection)
	{
		InputFixture f;
		f.layer.OnLeftStick(0.1f, 0.2f, 1.0);
		CHECK(f.layer.CurrentDirection(1.0) == Direction::kNeutral);
		f.layer.OnLeftStick(0.2f, -0.9f, 1.0);
		CHECK(f.layer.CurrentDirection(1.0) == Direction::kBack);
		f.layer.OnLeftStick(-0.8f, 0.3f, 1.0);
		CHECK(f.layer.CurrentDirection(1.0) == Direction::kLeft);
	}

	TEST(InputStickFlickChordLeniency)
	{
		InputFixture f;
		f.layer.OnLeftStick(0.0f, 0.9f, 1.00);
		f.layer.OnLeftStick(0.0f, 0.0f, 1.05);  // let go
		CHECK(f.layer.CurrentDirection(1.10) == Direction::kForward);  // 50 ms after
		CHECK(f.layer.CurrentDirection(1.30) == Direction::kNeutral);  // outside 120 ms
	}

	TEST(InputGamepadTriggerThreshold)
	{
		constexpr auto RT = keys::kGamepadRightTrigger;
		InputFixture f;
		f.layer.OnAnalogButton(RT, 0.2f, 1.00);  // resting finger: not a press
		f.layer.OnAnalogButton(RT, 0.0f, 1.05);
		CHECK(f.events.empty());

		f.layer.OnAnalogButton(RT, 0.6f, 2.00);  // press
		f.layer.OnAnalogButton(RT, 0.4f, 2.05);  // dips below threshold, above half: still held
		f.layer.OnAnalogButton(RT, 0.6f, 2.08);
		f.layer.OnAnalogButton(RT, 0.1f, 2.10);  // release
		CHECK(f.events.size() == 1);              // no chatter into a second attack
		CHECK(f.events.size() == 1 && f.events[0].action == Action::kLightAttack);
	}

	TEST(InputGamepadHoldAndStickIsDirectionalPower)
	{
		constexpr auto RT = keys::kGamepadRightTrigger;
		InputFixture f;
		f.layer.OnLeftStick(0.0f, -1.0f, 1.00);  // stick back
		f.layer.OnAnalogButton(RT, 1.0f, 1.01);
		f.layer.OnAnalogButton(RT, 1.0f, 1.20);  // held repeats from the engine
		f.layer.OnAnalogButton(RT, 1.0f, 1.35);  // past 300 ms
		f.layer.OnAnalogButton(RT, 0.0f, 1.50);
		CHECK(f.events.size() == 1);
		CHECK(f.events.size() == 1 && f.events[0].action == Action::kPowerAttack);
		CHECK(f.events.size() == 1 && f.events[0].direction == Direction::kBack);
	}

	TEST(InputDedicatedPowerKey)
	{
		InputConfig cfg;
		cfg.powerAttack = { 0x38 };  // LAlt
		InputFixture f(cfg);

		f.layer.OnButton(0x38, true, 1.0);
		f.layer.OnButton(0x38, true, 1.1);  // repeat does not re-fire
		f.layer.OnButton(0x38, false, 1.2);
		CHECK(f.events.size() == 1);
		CHECK(f.events[0].action == Action::kPowerAttack);

		// With a dedicated key, holding Attack stays a light attack.
		for (double t = 2.0; t <= 3.0; t += 0.1) {
			f.layer.OnButton(LMB, true, t);  // the engine repeats a held button every frame
		}
		f.layer.Update(3.0);
		f.layer.OnButton(LMB, false, 3.1);
		CHECK(f.events.size() == 2);
		CHECK(f.events[1].action == Action::kLightAttack);
	}

	TEST(InputLostReleaseExpiresWithoutAttack)
	{
		InputFixture f;
		f.layer.OnButton(LMB, true, 1.00);
		f.layer.OnButton(W, true, 1.00);
		// No repeats and no release (e.g. a menu opened). Much later:
		f.layer.Update(60.0);
		CHECK(f.events.empty());  // no bogus power attack
		CHECK(f.layer.CurrentDirection(60.0) == Direction::kNeutral);
		f.layer.OnButton(LMB, false, 60.1);  // the stray release is ignored too
		CHECK(f.events.empty());
	}

	TEST(InputBlockHoldStartsAndEnds)
	{
		constexpr auto LT = keys::kGamepadLeftTrigger;
		InputFixture f;
		f.layer.OnLeftStick(0.0f, -0.9f, 1.00);
		f.layer.OnAnalogButton(LT, 0.2f, 1.00);  // light touch: below TriggerThreshold
		CHECK(f.blocks.empty());
		f.layer.OnAnalogButton(LT, 0.8f, 1.02);
		f.layer.OnAnalogButton(LT, 0.8f, 1.10);  // held repeats don't re-fire
		f.layer.OnAnalogButton(LT, 0.8f, 1.50);
		CHECK(f.blocks.size() == 1 && f.blocks[0].active && f.blocks[0].direction == Direction::kBack);
		CHECK(f.layer.IsBlocking());
		f.layer.OnAnalogButton(LT, 0.0f, 1.60);
		CHECK(f.blocks.size() == 2 && !f.blocks[1].active);
		CHECK(!f.layer.IsBlocking());
		CHECK(f.events.empty());  // blocking is not an attack and never enters the buffer
		CHECK(!f.layer.PeekBuffered(1.60).has_value());

		const auto parsed = ParseConfig("[Input]\nBlock = Mouse2\n");
		CHECK(parsed.config.input.block == KeyList{ 0x101 });
	}

	TEST(InputLostBlockReleaseEndsBlock)
	{
		InputFixture f;
		f.layer.OnButton(0x101, true, 1.00);  // Mouse2
		CHECK(f.layer.IsBlocking());
		f.layer.Update(5.0);  // repeats stopped without a release
		CHECK(!f.layer.IsBlocking());
		CHECK(f.blocks.size() == 2 && !f.blocks[1].active);
	}

	TEST(InputBufferExpiresAndConsumes)
	{
		InputFixture f;
		f.layer.OnButton(LMB, true, 1.00);
		f.layer.OnButton(LMB, false, 1.05);
		CHECK(f.layer.PeekBuffered(1.10).has_value());
		CHECK(!f.layer.PeekBuffered(1.40).has_value());  // 350 ms > 250 ms window

		f.layer.OnButton(LMB, true, 2.00);
		f.layer.OnButton(LMB, false, 2.05);
		const auto first = f.layer.ConsumeBuffered(2.10);
		CHECK(first.has_value() && first->action == Action::kLightAttack);
		CHECK(!f.layer.ConsumeBuffered(2.11).has_value());
	}
}

int main()
{
	log::SetLevel(log::Level::kOff);  // keep test output to failures only

	for (const auto& test : Tests()) {
		const int before = g_failures;
		test.fn();
		std::printf("%s %s\n", g_failures == before ? "[ ok ]" : "[FAIL]", test.name);
	}
	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
