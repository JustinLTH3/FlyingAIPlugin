#include "FABound.h"
#include "FAWorldSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FABoundSubdivisionTest,
                                 "FlyingAIPlugin.FAUnitTest.BoundSubdivision",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::
                                 EngineFilter)

bool FABoundSubdivisionTest::RunTest(const FString& Parameters)
{
	AFABound* Bound = NewObject<AFABound>();
	FFANewNodeChildType Result;
	Bound->Subdivide(Result);
	for (auto& Child : Result.Children)
	{
		TestValid(TEXT("Child should be valid."), Child);
		TestEqual(
			TEXT("Child should have half of the parent's half extent."), Child->HalfExtent,
			Bound->GetHalfExtent() / 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FANodeSubdivisionTest, "FlyingAIPlugin.FAUnitTest.NodeSubdivision",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::
                                 EngineFilter)

bool FANodeSubdivisionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FFaNodeData> Node = MakeShared<FFaNodeData>();
	Node->Position = FVector(0, 0, 0);
	Node->HalfExtent = FVector(100);
	Node->Depth = 0;
	FFANewNodeChildType Result;
	UFAWorldSubsystem::Subdivide(Node, Result);
	for (int i = 0; i < 8; i++)
	{
		auto& Child = Result.Children[i];
		TestValid(TEXT("Child should be valid."), Child);
		TestEqual(
			TEXT("Child should have half of the parent's half extent."), Child->HalfExtent,
			Node->HalfExtent / 2);
		TestEqual(TEXT("Child should have depth 1 above parent."), Child->Depth, Node->Depth + 1);
		TestEqual(
			TEXT("Half Extent should be half of parent's"), Child->HalfExtent,
			Node->HalfExtent / 2);
		TestEqual(
			TEXT("X Position should be (-)Half Extent"), Child->Position.X,
			Node->Position.X + (i % 2 == 1 ? 1 : -1) * Child->HalfExtent.X);
		TestEqual(
			TEXT("Y Position should be (-)Half Extent"), Child->Position.Y,
			Node->Position.Y + ((i >> 1) % 2 == 1 ? 1 : -1) * Child->HalfExtent.Y);
		TestEqual(
			TEXT("Z Position should be (-)Half Extent"), Child->Position.Z,
			Node->Position.Z + ((i >> 2) % 2 == 1 ? 1 : -1) * Child->HalfExtent.Z);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAAABBOverlapTest, "FlyingAIPlugin.FAUnitTest.AABBOverlapTest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::
                                 EngineFilter)

bool FAAABBOverlapTest::RunTest(const FString& Parameters)
{
	FVector P1 = FVector(0, 0, 0);
	FVector P2 = FVector(100, 0, 0);
	FVector H1 = FVector(100);
	FVector H2 = FVector(100);
	TestTrue(TEXT("Normal Case"), UFAWorldSubsystem::AABBOverlap(P1, P2, H1, H2));
	TestTrue(TEXT("Normal Case"), UFAWorldSubsystem::AABBOverlap(P2, P1, H2, H1));
	P1.X = -100;
	TestTrue(TEXT("Touch edge"), UFAWorldSubsystem::AABBOverlap(P1, P2, H1, H2));
	TestTrue(TEXT("Touch edge"), UFAWorldSubsystem::AABBOverlap(P2, P1, H2, H1));
	P1.X = -101;
	TestFalse(TEXT("No Overlap"), UFAWorldSubsystem::AABBOverlap(P1, P2, H1, H2));
	TestFalse(TEXT("No Overlap"), UFAWorldSubsystem::AABBOverlap(P2, P1, H2, H1));
	P1.X = -100;
	P1.Y = -100;
	P1.Z = -100;
	P2.X = 100;
	P2.Y = 100;
	P2.Z = 100;
	TestTrue(TEXT("Touching Corner"), UFAWorldSubsystem::AABBOverlap(P1, P2, H1, H2));
	TestTrue(TEXT("Touching Corner"), UFAWorldSubsystem::AABBOverlap(P2, P1, H2, H1));

	return true;
}
