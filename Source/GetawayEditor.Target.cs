using UnrealBuildTool;
using System.Collections.Generic;

public class GetawayEditorTarget : TargetRules
{
	public GetawayEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Getaway");
	}
}
