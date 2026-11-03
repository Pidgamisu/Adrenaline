using UnrealBuildTool;

public class GetawayMovement : ModuleRules
{
	public GetawayMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Niagara",
			"ProceduralMeshComponent",
			// The front end. UMG for the menu panel, and Slate/SlateCore for the style structs it
			// fills in by hand: the menu is built entirely in C++, with no Widget Blueprint asset.
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}
