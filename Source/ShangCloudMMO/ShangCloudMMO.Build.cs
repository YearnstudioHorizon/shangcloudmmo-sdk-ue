using UnrealBuildTool;

public class ShangCloudMMO : ModuleRules
{
	public ShangCloudMMO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking",
			"HTTP",
			"Json",
			"JsonUtilities",
			"WebSockets",
			"OpenSSL"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
