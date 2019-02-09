#include "CppUnitTest.h"
#include "../engine/engine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace BlendXChess;

namespace EngineTest
{
	TEST_CLASS(Engine)
	{
	public:
		TEST_CLASS_INITIALIZE(Init)
		{
			Game::initialize();
		}
		TEST_METHOD(Perft)
		{
			Position pos;
			Assert::AreEqual(1, pos.perft(0));
			Assert::AreEqual(20, pos.perft(1));
			Assert::AreEqual(400, pos.perft(2));
			Assert::AreEqual(8902, pos.perft(3));
			Assert::AreEqual(197281, pos.perft(4));
			Assert::AreEqual(4865609, pos.perft(5));
		}
	};
}