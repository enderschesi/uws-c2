#pragma once

#include <Net/funcs.h>
#include <Gameplay/helper.h>
#include <Gameplay/inventory.h>

#include <unordered_map>

// Includes building and editing..

inline bool ServerCreateBuildingActorHook(UObject* Controller, UFunction* Function, void* Parameters)
{
	if (Controller && Parameters)
	{
		static auto WoodItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/WoodItemData.WoodItemData"));
		static auto StoneItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/StoneItemData.StoneItemData"));
		static auto MetalItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/MetalItemData.MetalItemData"));

		UObject* MatDefinition = nullptr;
		UObject* MatInstance = nullptr;

		auto Pawn = Helper::GetPawnFromController(Controller);

		UObject* BuildingClass = nullptr;
		FVector BuildingLocation;
		FRotator BuildingRotation;
		bool bMirrored;

		if (FnVerDouble > 8)
		{
			struct FCreateBuildingActorData
			{
				uint32_t                                           BuildingClassHandle;                                      // 0x0000(0x0004) (ZeroConstructor, Transient, IsPlainOldData)
				struct FVector				                       BuildLoc;                                                 // 0x0004(0x000C) (Transient)
				struct FRotator                                    BuildRot;                                                 // 0x0010(0x000C) (ZeroConstructor, Transient, IsPlainOldData)
				bool                                               bMirrored;                                                // 0x001C(0x0001) (ZeroConstructor, Transient, IsPlainOldData)
			};

			struct SCBAParams { FCreateBuildingActorData CreateBuildingData; };
			auto Params = (SCBAParams*)Parameters;

			static auto BroadcastRemoteClientInfoOffset = GetOffset(Controller, "BroadcastRemoteClientInfo");
			auto RemoteClientInfo = (UObject**)(__int64(Controller) + BroadcastRemoteClientInfoOffset);

			if (RemoteClientInfo && *RemoteClientInfo)
			{
				static auto RemoteBuildableClassOffset = GetOffset(*RemoteClientInfo, "RemoteBuildableClass");
				auto bBuildingClass = (UObject**)(__int64(*RemoteClientInfo) + RemoteBuildableClassOffset);

				BuildingClass = bBuildingClass ? *bBuildingClass : nullptr;
				BuildingLocation = Params->CreateBuildingData.BuildLoc;
				BuildingRotation = Params->CreateBuildingData.BuildRot;
				bMirrored = Params->CreateBuildingData.bMirrored;

				/* static auto RemoteBuildingMaterialOffset = GetOffset(*RemoteClientInfo, "RemoteBuildingMaterial");
				auto RemoteBuildingMaterial = (TEnumAsByte<EFortResourceType>*)(__int64(*RemoteClientInfo) + RemoteBuildingMaterialOffset);

				switch (RemoteBuildingMaterial ? RemoteBuildingMaterial->Get() : EFortResourceType::None)
				{
				case EFortResourceType::Wood:
					MatDefinition = WoodItemData;
					break;
				case EFortResourceType::Stone:
					MatDefinition = WoodItemData;
					break;
				case EFortResourceType::Metal:
					MatDefinition = WoodItemData;
					break;
				} */
			}
		}
		else
		{
			struct FBuildingClassData {
				UObject* BuildingClass;
				int                                                PreviousBuildingLevel;                                    // 0x0008(0x0004) (ZeroConstructor, Transient, IsPlainOldData)
				int                                                UpgradeLevel;
			};

			struct SCBAParams {
				FBuildingClassData BuildingClassData; // FBuildingClassData&
				FVector BuildLoc;
				FRotator BuildRot;
				bool bMirrored;
			};

			auto Params = (SCBAParams*)Parameters;

			BuildingClass = Params->BuildingClassData.BuildingClass;
			BuildingLocation = Params->BuildLoc;
			BuildingRotation = Params->BuildRot;
			bMirrored = Params->bMirrored;
		}

		if (BuildingClass)
		{
			constexpr bool bUseAnticheat = false;

			if (bUseAnticheat && !validBuildingClass(BuildingClass))
				return false;

			if (!MatDefinition) // this should only happen on >s7
			{
				auto BuildingClassName = BuildingClass->GetFullName();

				// TODO: figure out a better way

				if (BuildingClassName.contains(("W1")))
					MatDefinition = WoodItemData;
				else if (BuildingClassName.contains(("S1")))
					MatDefinition = StoneItemData;
				else if (BuildingClassName.contains(("M1")))
					MatDefinition = MetalItemData;
			}

			if (MatDefinition)
			{
				MatInstance = Inventory::FindItemInInventory(Controller, MatDefinition);

				if (!MatInstance || *FFortItemEntry::GetCount(GetItemEntryFromInstance(MatInstance)) < 10)
					return false;

				static __int64 (*CantBuild)(UObject*, UObject*, FVector, FRotator, char, void*, void*) = nullptr;

				// CantBuild = decltype(CantBuild)(FindPattern("48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 41 56 48 83 EC ? 49 8B E9 4D 8B F0"));

				int a = 0;

				// if (CantBuild && !CantBuild(Helper::GetWorld(), BuildingClass, BuildingLocation, BuildingRotation, bMirrored, &a, &a))
				{
					UObject* BuildingActor = Easy::SpawnActor(BuildingClass, BuildingLocation, BuildingRotation);

					if (BuildingActor)
					{
						bool bSuccessful = false;

						if (!bUseAnticheat || validBuild(BuildingActor, Pawn))
						{
							// Helper::IDoNotKnow(BuildingActor);

							auto PlayerState = Helper::GetPlayerStateFromController(Controller);

							static auto TeamOffset = GetOffset(BuildingActor, "Team");

							auto PlayersTeamIndex = *Teams::GetTeamIndex(PlayerState);;

							if (TeamOffset != -1)
							{
								auto Team = (TEnumAsByte<EFortTeam>*)(__int64(BuildingActor) + TeamOffset);
								*Team = PlayersTeamIndex; // *PlayerState->Member<TEnumAsByte<EFortTeam>>("Team");
							}

							static auto Building_TeamIndexOffset = GetOffset(BuildingActor, "TeamIndex");
							
							if (Building_TeamIndexOffset != -1)
							{
								auto TeamIndex = (uint8_t*)(__int64(BuildingActor) + Building_TeamIndexOffset);
								*TeamIndex = PlayersTeamIndex;
							}

							// Helper::SetMirrored(BuildingActor, bMirrored);
							Helper::InitializeBuildingActor(Controller, BuildingActor, true);

							bSuccessful = true;

							// if (!Helper::IsStructurallySupported(BuildingActor))
								// bSuccessful = false;
						}

						if (!bSuccessful)
						{
							Helper::SetActorScale3D(BuildingActor, {});
							Helper::SilentDie(BuildingActor);
						}
						else
						{
							if (!bIsPlayground)
							{
								Inventory::DecreaseItemCount(Controller, MatInstance, 10);
							}
						}
					}
				}
			}
		}
	}

	return false;
}

inline bool ServerBeginEditingBuildingActorHook(UObject* Controller, UFunction* Function, void* Parameters)
{
	struct Parms { UObject* BuildingActor; };
	static UObject* EditToolDefinition = FindObject(("FortEditToolItemDefinition /Game/Items/Weapons/BuildingTools/EditTool.EditTool"));

	auto EditToolInstance = Inventory::FindItemInInventory(Controller, EditToolDefinition);

	auto Pawn = Helper::GetPawnFromController(Controller);

	UObject* BuildingToEdit = ((Parms*)Parameters)->BuildingActor;

	if (Controller && BuildingToEdit)
	{
		// TODO: Check TeamIndex of BuildingToEdit and the Controller
		// TODO: Add distance check

		if (EditToolInstance)
		{
			auto EditTool = Inventory::EquipWeaponDefinition(Pawn, EditToolDefinition, Inventory::GetItemGuid(EditToolInstance));

			if (EditTool)
			{
				auto PlayerState = *Controller->CachedMember<UObject*>(("PlayerState"));
				*BuildingToEdit->CachedMember<UObject*>(("EditingPlayer")) = PlayerState;
				static auto OnRep_EditingPlayer = BuildingToEdit->Function(("OnRep_EditingPlayer"));

				if (OnRep_EditingPlayer)
					BuildingToEdit->ProcessEvent(OnRep_EditingPlayer);

				*EditTool->CachedMember<UObject*>(("EditActor")) = BuildingToEdit;
				static auto OnRep_EditActor = EditTool->Function(("OnRep_EditActor"));

				if (OnRep_EditActor)
					EditTool->ProcessEvent(OnRep_EditActor);
			}
			else
				std::cout << "Failed to equip edittool??\n";
		}
		else
			std::cout << ("No Edit Tool Instance?\n");
	}

	return false;
}

/* inline bool ServerEditBuildingActorHook(UObject* Controller, UFunction* Function, void* Parameters)
{
	struct Parms {
		UObject* BuildingActorToEdit;                                      // (Parm, ZeroConstructor, IsPlainOldData)
		UObject* NewBuildingClass;                                         // (Parm, ZeroConstructor, IsPlainOldData)
		int                                                RotationIterations;                                       // (Parm, ZeroConstructor, IsPlainOldData)
		bool                                               bMirrored;                                                // (Parm, ZeroConstructor, IsPlainOldData)
	};

	auto Params = (Parms*)Parameters;

	if (Params && Controller)
	{
		auto BuildingActor = Params->BuildingActorToEdit;
		auto NewBuildingClass = Params->NewBuildingClass;

		static auto bMirroredOffset = FindOffsetStruct("Function /Script/FortniteGame.FortPlayerController.ServerEditBuildingActor", "bMirrored");
		auto bMirrored = *(bool*)(__int64(Parameters) + bMirroredOffset);

		static auto RotationIterationsOffset = FindOffsetStruct("Function /Script/FortniteGame.FortPlayerController.ServerEditBuildingActor", "RotationIterations");
		auto RotationIterations = *(int*)(__int64(Parameters) + RotationIterationsOffset);

		if (BuildingActor && NewBuildingClass)
		{
			// TODO: Check EditTool's EditActor and make sure its the BuildingActor
			
			// bool bDestroyed = readBitfield(BuildingActor, "bDestroyed");

			if (RotationIterations > 3)
				return false;

			Helper::ReplaceBuildingActor(Controller, BuildingActor, NewBuildingClass, RotationIterations, bMirrored);
		}
	}

	return false;
} */

inline bool ServerEditBuildingActorHook(UObject* Controller, UFunction* Function, void* Parameters)
{
	struct Parms {
		UObject* BuildingActorToEdit;
		UObject* NewBuildingClass;
		int RotationIterations;
		bool bMirrored;
	};

	auto Params = (Parms*)Parameters;

	if (Params && Controller)
	{
		auto BuildingActor = Params->BuildingActorToEdit;
		auto NewBuildingClass = Params->NewBuildingClass;

		static auto bMirroredOffset = FindOffsetStruct("Function /Script/FortniteGame.FortPlayerController.ServerEditBuildingActor", "bMirrored");
		auto bMirrored = *(bool*)(__int64(Parameters) + bMirroredOffset);

		static auto RotationIterationsOffset = FindOffsetStruct("Function /Script/FortniteGame.FortPlayerController.ServerEditBuildingActor", "RotationIterations");
		auto RotationIterations = *(int*)(__int64(Parameters) + RotationIterationsOffset);

		if (BuildingActor && NewBuildingClass)
		{
			if (RotationIterations > 3)
				return false;

			auto Location = Helper::GetActorLocation(BuildingActor);
			auto Rotation = Helper::GetActorRotation(BuildingActor);

			static bool bFound = false;

			static auto BuildingSMActorReplaceBuildingActorAddr = FindPattern("4C 8B DC 55 57 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 48 8B 85 ? ? ? ? 33 FF 40 38 3D ? ? ? ?");

			if (!bFound)
			{
				bFound = true;

				if (Engine_Version >= 426 && !BuildingSMActorReplaceBuildingActorAddr)
					BuildingSMActorReplaceBuildingActorAddr = FindPattern("48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 ? 44 0F 29 48 ? 44 0F 29 90 ? ? ? ? 44 0F 29 B8 ? ? ? ? 48 8B 05");

				if (!BuildingSMActorReplaceBuildingActorAddr || Engine_Version <= 421)
					BuildingSMActorReplaceBuildingActorAddr = FindPattern("48 8B C4 44 89 48 20 55 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 48 89 70 E8 33 FF 40 38 3D ? ? ? ? 48 8B F1 4C 89 60 E0 44 8B E2");


				if (!BuildingSMActorReplaceBuildingActorAddr)
					BuildingSMActorReplaceBuildingActorAddr = FindPattern("48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 ? 44 0F 29 48 ? 44 0F 29 90 ? ? ? ? 44 0F 29 B8 ? ? ? ? 48 8B 05");

				if (!BuildingSMActorReplaceBuildingActorAddr) // c3
					BuildingSMActorReplaceBuildingActorAddr = FindPattern("48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 ? 44 0F 29 48 ? 44 0F 29 90 ? ? ? ? 48 8B 05 ? ? ? ?");
			}

			static UObject* (__fastcall* BuildingSMActorReplaceBuildingActor)(UObject* BuildingSMActor, unsigned int a2, UObject * a3, unsigned int a4, int a5, unsigned __int8 bMirrored, UObject* Controller)
				= decltype(BuildingSMActorReplaceBuildingActor)(BuildingSMActorReplaceBuildingActorAddr);

			if (BuildingSMActorReplaceBuildingActor)
				BuildingSMActorReplaceBuildingActor(BuildingActor, 1, NewBuildingClass, 0, RotationIterations, bMirrored, Controller);
			else
				std::cout << "No BuildingSMActorReplaceBuildingActor!\n";
		}
	}

	return false;
}


inline bool ServerEndEditingBuildingActorHook(UObject* Controller, UFunction* Function, void* Parameters)
{
	if (Controller && Parameters && !Helper::IsInAircraft(Controller))
	{
		struct Parms {
			UObject* BuildingActorToStopEditing;
		};

		auto Params = (Parms*)Parameters;

		auto Pawn = Helper::GetPawnFromController(Controller);

		// TODO: Check BuildingActorToStopEditing->EditingPlayer and make sure its our player.

		if (Pawn)
		{
			auto CurrentWep = Helper::GetCurrentWeapon(Pawn);

			if (CurrentWep)
			{
				auto CurrentWepItemDef = Helper::GetWeaponData(CurrentWep);
				static UObject* EditToolDefinition = FindObject(("FortEditToolItemDefinition /Game/Items/Weapons/BuildingTools/EditTool.EditTool"));

				if (CurrentWepItemDef == EditToolDefinition) // Player CONFIRMED the edit
				{
					// auto EditToolInstance = Inventory::FindItemInInventory(Controller, EditToolDefinition);
					auto EditTool = CurrentWep;// Inventory::EquipWeaponDefinition(*Pawn, EditToolDefinition, Inventory::GetItemGuid(EditToolInstance));

					*EditTool->CachedMember<bool>(("bEditConfirmed")) = true;
					*EditTool->CachedMember<UObject*>(("EditActor")) = nullptr;

					static auto OnRep_EditActorFn = EditTool->Function(("OnRep_EditActor"));

					if (OnRep_EditActorFn)
						EditTool->ProcessEvent(OnRep_EditActorFn);
				}
			}
		}

		if (Params->BuildingActorToStopEditing)
		{
			*Params->BuildingActorToStopEditing->CachedMember<UObject*>(("EditingPlayer")) = nullptr;
			static auto OnRep_EditingPlayer = Params->BuildingActorToStopEditing->Function(("OnRep_EditingPlayer"));

			if (OnRep_EditingPlayer)
				Params->BuildingActorToStopEditing->ProcessEvent(OnRep_EditingPlayer);
		}
	}

	return false;
}

void InitializeBuildHooks()
{
	// if (Engine_Version < 426)
	{
		AddHook(("Function /Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor"), ServerCreateBuildingActorHook);

		// if (Engine_Version < 424)
		{
			AddHook(("Function /Script/FortniteGame.FortPlayerController.ServerBeginEditingBuildingActor"), ServerBeginEditingBuildingActorHook);
			AddHook(("Function /Script/FortniteGame.FortPlayerController.ServerEditBuildingActor"), ServerEditBuildingActorHook);
			AddHook(("Function /Script/FortniteGame.FortPlayerController.ServerEndEditingBuildingActor"), ServerEndEditingBuildingActorHook);
		}
	}
}