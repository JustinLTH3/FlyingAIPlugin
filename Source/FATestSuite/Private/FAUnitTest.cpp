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
	for (auto& Child : Result.Children)
	{
		TestValid(TEXT("Child should be valid."), Child);
		TestEqual(
			TEXT("Child should have half of the parent's half extent."), Child->HalfExtent,
			Node->HalfExtent / 2);
		TestEqual(TEXT("Child should have depth 1 above parent."), Child->Depth, Node->Depth + 1);
	}

	return true;
}
