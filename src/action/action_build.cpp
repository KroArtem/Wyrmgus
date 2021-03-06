//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name action_build.cpp - The build building action. */
//
//      (c) Copyright 1998-2015 by Lutz Sammer, Jimmy Salmon,
//      Russell Smith and Andrettin
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "action/action_build.h"

#include "action/action_built.h"
#include "ai.h"
#include "animation.h"
//Wyrmgus start
#include "character.h"
#include "commands.h"
//Wyrmgus end
#include "iolib.h"
#include "map.h"
#include "pathfinder.h"
#include "player.h"
#include "script.h"
//Wyrmgus start
#include "tileset.h"
//Wyrmgus end
#include "translate.h"
#include "ui.h"
#include "unit.h"
//Wyrmgus start
#include "unit_find.h"
//Wyrmgus end
#include "unittype.h"
#include "video.h"

//Wyrmgus start
#include "../ai/ai_local.h"
//Wyrmgus end

extern void AiReduceMadeInBuilt(PlayerAi &pai, const CUnitType &type);

enum {
	State_Start = 0,
	State_MoveToLocationMax = 10, // Range from prev
	State_NearOfLocation = 11, // Range to next
	State_StartBuilding_Failed = 20,
	State_BuildFromInside = 21,
	State_BuildFromOutside = 22
};



/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

//Wyrmgus start
///* static */ COrder *COrder::NewActionBuild(const CUnit &builder, const Vec2i &pos, CUnitType &building)
/* static */ COrder *COrder::NewActionBuild(const CUnit &builder, const Vec2i &pos, CUnitType &building, int z)
//Wyrmgus end
{
	//Wyrmgus start
//	Assert(Map.Info.IsPointOnMap(pos));
	Assert(Map.Info.IsPointOnMap(pos, z));
	//Wyrmgus end

	COrder_Build *order = new COrder_Build;

	order->goalPos = pos;
	//Wyrmgus start
	order->MapLayer = z;
	//Wyrmgus end
	if (building.BoolFlag[BUILDEROUTSIDE_INDEX].value) {
		order->Range = builder.Type->RepairRange;
	} else {
		// If building inside, but be next to stop
		if (building.BoolFlag[SHOREBUILDING_INDEX].value && builder.Type->UnitType == UnitTypeLand) {
			// Peon won't dive :-)
			order->Range = 1;
		}
	}
	order->Type = &building;
	return order;
}


/* virtual */ void COrder_Build::Save(CFile &file, const CUnit &unit) const
{
	file.printf("{\"action-build\",");

	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"range\", %d,", this->Range);
	file.printf(" \"tile\", {%d, %d},", this->goalPos.x, this->goalPos.y);
	//Wyrmgus start
	file.printf(" \"map-layer\", %d,", this->MapLayer);
	//Wyrmgus end

	if (this->BuildingUnit != NULL) {
		file.printf(" \"building\", \"%s\",", UnitReference(this->BuildingUnit).c_str());
	}
	file.printf(" \"type\", \"%s\",", this->Type->Ident.c_str());
	file.printf(" \"state\", %d", this->State);
	file.printf("}");
}

/* virtual */ bool COrder_Build::ParseSpecificData(lua_State *l, int &j, const char *value, const CUnit &unit)
{
	if (!strcmp(value, "building")) {
		++j;
		lua_rawgeti(l, -1, j + 1);
		this->BuildingUnit = CclGetUnitFromRef(l);
		lua_pop(l, 1);
	} else if (!strcmp(value, "range")) {
		++j;
		this->Range = LuaToNumber(l, -1, j + 1);
	} else if (!strcmp(value, "state")) {
		++j;
		this->State = LuaToNumber(l, -1, j + 1);
	} else if (!strcmp(value, "tile")) {
		++j;
		lua_rawgeti(l, -1, j + 1);
		CclGetPos(l, &this->goalPos.x , &this->goalPos.y);
		lua_pop(l, 1);
	} else if (!strcmp(value, "type")) {
		++j;
		this->Type = UnitTypeByIdent(LuaToString(l, -1, j + 1));
	//Wyrmgus start
	} else if (!strcmp(value, "map-layer")) {
		++j;
		this->MapLayer = LuaToNumber(l, -1, j + 1);
	//Wyrmgus end
	} else {
		return false;
	}
	return true;
}

/* virtual */ bool COrder_Build::IsValid() const
{
	return true;
}

/* virtual */ PixelPos COrder_Build::Show(const CViewport &vp, const PixelPos &lastScreenPos) const
{
	//Wyrmgus start
	if (this->MapLayer != CurrentMapLayer) {
		return lastScreenPos;
	}
	//Wyrmgus end

	PixelPos targetPos = vp.TilePosToScreen_Center(this->goalPos);
	targetPos.x += (this->GetUnitType().TileWidth - 1) * PixelTileSize.x / 2;
	targetPos.y += (this->GetUnitType().TileHeight - 1) * PixelTileSize.y / 2;

	const int w = this->GetUnitType().BoxWidth;
	const int h = this->GetUnitType().BoxHeight;
	DrawSelection(ColorGray, targetPos.x - w / 2, targetPos.y - h / 2, targetPos.x + w / 2, targetPos.y + h / 2);
	//Wyrmgus start
//	Video.FillCircleClip(ColorGreen, lastScreenPos, 2);
//	Video.DrawLineClip(ColorGreen, lastScreenPos, targetPos);
//	Video.FillCircleClip(ColorGreen, targetPos, 3);
	if (Preference.ShowPathlines) {
		Video.FillCircleClip(ColorGreen, lastScreenPos, 2);
		Video.DrawLineClip(ColorGreen, lastScreenPos, targetPos);
		Video.FillCircleClip(ColorGreen, targetPos, 3);
	}
	//Wyrmgus end
	return targetPos;
}

/* virtual */ void COrder_Build::UpdatePathFinderData(PathFinderInput &input)
{
	input.SetMinRange(this->Type->BoolFlag[BUILDEROUTSIDE_INDEX].value ? 1 : 0);
	input.SetMaxRange(this->Range);

	const Vec2i tileSize(this->Type->TileWidth, this->Type->TileHeight);
	//Wyrmgus start
//	input.SetGoal(this->goalPos, tileSize);
	input.SetGoal(this->goalPos, tileSize, this->MapLayer);
	//Wyrmgus end
}

/** Called when unit is killed.
**  warn the AI module.
*/
void COrder_Build::AiUnitKilled(CUnit &unit)
{
	DebugPrint("%d: %d(%s) killed, with order %s!\n" _C_
			   unit.Player->Index _C_ UnitNumber(unit) _C_
			   unit.Type->Ident.c_str() _C_ this->Type->Ident.c_str());
	if (this->BuildingUnit == NULL) {
		AiReduceMadeInBuilt(*unit.Player->Ai, *this->Type);
	}
}



/**
**  Move to build location
**
**  @param unit  Unit to move
*/
bool COrder_Build::MoveToLocation(CUnit &unit)
{
	// First entry
	if (this->State == 0) {
		unit.pathFinderData->output.Cycles = 0; //moving counter
		this->State = 1;
	}
	switch (DoActionMove(unit)) { // reached end-point?
		case PF_UNREACHABLE: {
			//Wyrmgus start
			if ((Map.Field(unit.tilePos, unit.MapLayer)->Flags & MapFieldBridge) && !unit.Type->BoolFlag[BRIDGE_INDEX].value && unit.Type->UnitType == UnitTypeLand) {
				std::vector<CUnit *> table;
				Select(unit.tilePos, unit.tilePos, table, unit.MapLayer);
				for (size_t i = 0; i != table.size(); ++i) {
					if (!table[i]->Removed && table[i]->Type->BoolFlag[BRIDGE_INDEX].value && table[i]->CanMove()) {
						if (table[i]->CurrentAction() == UnitActionStill) {
							CommandStopUnit(*table[i]);
							CommandMove(*table[i], this->goalPos, FlushCommands, this->MapLayer);
						}
						return false;
					}
				}
			}
			//Wyrmgus end
			// Some tries to reach the goal
			if (this->State++ < State_MoveToLocationMax) {
				// To keep the load low, retry each 1/4 second.
				// NOTE: we can already inform the AI about this problem?
				unit.Wait = CYCLES_PER_SECOND / 4;
				return false;
			}

			//Wyrmgus start
//			unit.Player->Notify(NotifyYellow, unit.tilePos, "%s", _("You cannot reach building place"));
			unit.Player->Notify(NotifyYellow, unit.tilePos, unit.MapLayer, _("%s cannot reach building place"), unit.GetMessageName().c_str());
			//Wyrmgus end
			if (unit.Player->AiEnabled) {
				AiCanNotReach(unit, this->GetUnitType());
			}
			return true;
		}
		case PF_REACHED:
			this->State = State_NearOfLocation;
			return false;

		default:
			// Moving...
			return false;
	}
}

static bool CheckLimit(const CUnit &unit, const CUnitType &type)
{
	const CPlayer &player = *unit.Player;
	bool isOk = true;

	// Check if enough resources for the building.
	if (player.CheckUnitType(type)) {
		// FIXME: Better tell what is missing?
		player.Notify(NotifyYellow, unit.tilePos,
					  //Wyrmgus start
					  unit.MapLayer,
//					  _("Not enough resources to build %s"), type.Name.c_str());
					  _("Not enough resources to build %s"), type.GetDefaultName(Players[player.Index]).c_str());
					  //Wyrmgus end
		isOk = false;
	}

	// Check if hiting any limits for the building.
	if (player.CheckLimits(type) < 0) {
		player.Notify(NotifyYellow, unit.tilePos,
					  //Wyrmgus start
					  unit.MapLayer,
//					  _("Can't build more units %s"), type.Name.c_str());
					  _("Can't build more units %s"), type.GetDefaultName(Players[player.Index]).c_str());
					  //Wyrmgus end
		isOk = false;
	}
	if (isOk == false && player.AiEnabled) {
		AiCanNotBuild(unit, type);
	}
	return isOk;
}


class AlreadyBuildingFinder
{
public:
	AlreadyBuildingFinder(const CUnit &unit, const CUnitType &t) :
		worker(&unit), type(&t) {}
	bool operator()(const CUnit *const unit) const
	{
		return (!unit->Destroyed && unit->Type == type
				&& (worker->Player == unit->Player || worker->IsAllied(*unit)));
	}
	CUnit *Find(const CMapField *const mf) const
	{
		return mf->UnitCache.find(*this);
	}
private:
	const CUnit *worker;
	const CUnitType *type;
};

/**
**  Check if the unit can build
**
**  @param unit  Unit to check
**
**  @return OnTop or NULL
*/
CUnit *COrder_Build::CheckCanBuild(CUnit &unit) const
{
	const Vec2i pos = this->goalPos;
	const CUnitType &type = this->GetUnitType();

	// Check if the building could be built there.

	//Wyrmgus start
//	CUnit *ontop = CanBuildUnitType(&unit, type, pos, 1);
	bool ignore_exploration = unit.Player->AiEnabled;
	CUnit *ontop = CanBuildUnitType(&unit, type, pos, 1, ignore_exploration, this->MapLayer);
	//Wyrmgus end

	if (ontop != NULL) {
		return ontop;
	}
	return NULL;
}

/**
**  Replaces this build command with repair.
**
**  @param unit     Builder who got this build order
**  @param building Building to repair
*/
void COrder_Build::HelpBuild(CUnit &unit, CUnit &building)
{
//Wyrmgus start
//#if 0
//Wyrmgus end
	/*
	 * FIXME: rb - CheckAlreadyBuilding should be somehow
	 * enabled/disable via game lua scripting
	 */
	if (unit.CurrentOrder() == this) {
		  DebugPrint("%d: Worker [%d] is helping build: %s [%d]\n"
		  //Wyrmgus start
//		  _C_ unit.Player->Index _C_ unit.Slot
//		  _C_ building->Type->Name.c_str()
//		  _C_ building->Slot);
		  _C_ unit.Player->Index _C_ UnitNumber(unit)
		  _C_ building.Type->GetDefaultName(*unit.Player).c_str()
		  _C_ UnitNumber(building));
		  //Wyrmgus end

		// shortcut to replace order, without inserting and removing in front of Orders
		delete this; // Bad
		unit.Orders[0] = COrder::NewActionRepair(unit, building);
		return ;
	}
//Wyrmgus start
//#endif
//Wyrmgus end
}


bool COrder_Build::StartBuilding(CUnit &unit, CUnit &ontop)
{
	const CUnitType &type = this->GetUnitType();

	unit.Player->SubUnitType(type);

	CUnit *build = MakeUnit(const_cast<CUnitType &>(type), unit.Player);
	
	//Wyrmgus start
	build->Name.clear(); // under construction buildings should have no proper name
	//Wyrmgus end
	
	// If unable to make unit, stop, and report message
	if (build == NULL) {
		// FIXME: Should we retry this?
		unit.Player->Notify(NotifyYellow, unit.tilePos,
							//Wyrmgus start
							unit.MapLayer,
//							_("Unable to create building %s"), type.Name.c_str());
							_("Unable to create building %s"), type.GetDefaultName(*unit.Player).c_str());
							//Wyrmgus end
		if (unit.Player->AiEnabled) {
			AiCanNotBuild(unit, type);
		}
		return false;
	}
	build->Constructed = 1;
	build->CurrentSightRange = 0;

	// Building on top of something, may remove what is beneath it
	if (&ontop != &unit) {
		CBuildRestrictionOnTop *b;

		//Wyrmgus start
//		b = static_cast<CBuildRestrictionOnTop *>(OnTopDetails(*build, ontop.Type));
		b = static_cast<CBuildRestrictionOnTop *>(OnTopDetails(*build->Type, ontop.Type));
		//Wyrmgus end
		Assert(b);
		if (b->ReplaceOnBuild) {
			//Wyrmgus start
//			build->ResourcesHeld = ontop.ResourcesHeld; // We capture the value of what is beneath.
			build->SetResourcesHeld(ontop.ResourcesHeld); // We capture the value of what is beneath.
			//Wyrmgus end
			build->Variable[GIVERESOURCE_INDEX].Value = ontop.Variable[GIVERESOURCE_INDEX].Value;
			build->Variable[GIVERESOURCE_INDEX].Max = ontop.Variable[GIVERESOURCE_INDEX].Max;
			build->Variable[GIVERESOURCE_INDEX].Enable = ontop.Variable[GIVERESOURCE_INDEX].Enable;
			//Wyrmgus start
			if (ontop.Unique != NULL) {
				build->SetUnique(ontop.Unique);
			} else {
				if (ontop.Prefix != NULL) {
					build->SetPrefix(ontop.Prefix);
				}
				if (ontop.Suffix != NULL) {
					build->SetSuffix(ontop.Suffix);
				}
				if (ontop.Spell != NULL) {
					build->SetSpell(ontop.Spell);
				}
			}
			//Wyrmgus end
			ontop.Remove(NULL); // Destroy building beneath
			UnitLost(ontop);
			UnitClearOrders(ontop);
			ontop.Release();
		}
	}

	// Must set action before placing, otherwise it will incorrectly mark radar
	delete build->CurrentOrder();
	build->Orders[0] = COrder::NewActionBuilt(unit, *build);

	UpdateUnitSightRange(*build);
	// Must place after previous for map flags
	//Wyrmgus start
//	build->Place(this->goalPos);
	build->Place(this->goalPos, this->MapLayer);
	//Wyrmgus end

	//Wyrmgus start
	build->Player->NumBuildingsUnderConstruction++;
	//Wyrmgus end

	// HACK: the building is not ready yet
	build->Player->UnitTypesCount[type.Slot]--;
	if (build->Active) {
		build->Player->UnitTypesAiActiveCount[type.Slot]--;
	}
	//Wyrmgus start
	if (build->Type->BoolFlag[TOWNHALL_INDEX].value) {
		build->Player->NumTownHalls--;
	}
	if (build->Character == NULL) {
		build->Player->UnitTypesNonHeroCount[type.Slot]--;
	} else {
		build->Player->Heroes.erase(std::remove(build->Player->Heroes.begin(), build->Player->Heroes.end(), build), build->Player->Heroes.end());
	}
	
	if (build->Player->AiEnabled && build->Player->Ai && std::find(build->Player->Ai->Scouts.begin(), build->Player->Ai->Scouts.end(), build) != build->Player->Ai->Scouts.end()) {
		build->Player->Ai->Scouts.erase(std::remove(build->Player->Ai->Scouts.begin(), build->Player->Ai->Scouts.end(), build), build->Player->Ai->Scouts.end());
	}
	//Wyrmgus end

	// We need somebody to work on it.
	if (!type.BoolFlag[BUILDEROUTSIDE_INDEX].value) {
		//Wyrmgus start
//		UnitShowAnimation(unit, unit.Type->Animations->Still);
		UnitShowAnimation(unit, unit.GetAnimations()->Still);
		//Wyrmgus end
		unit.Remove(build);
		this->State = State_BuildFromInside;
		if (unit.Selected) {
			SelectedUnitChanged();
		}
	} else {
		this->State = State_BuildFromOutside;
		this->BuildingUnit = build;
		//Wyrmgus start
//		unit.Direction = DirectionToHeading(build->tilePos - unit.tilePos);
//		UnitUpdateHeading(unit);
		const Vec2i dir = Vec2i(build->tilePos.x * PixelTileSize.x, build->tilePos.y * PixelTileSize.y) + build->Type->GetHalfTilePixelSize() - Vec2i(unit.tilePos.x * PixelTileSize.x, unit.tilePos.y * PixelTileSize.y) - unit.Type->GetHalfTilePixelSize();
		UnitHeadingFromDeltaXY(unit, dir);
		//Wyrmgus end
	}
	return true;
}

//Wyrmgus start
void COrder_Build::ConvertUnitType(const CUnit &unit, CUnitType &newType)
{
	this->Type = &newType;
}
//Wyrmgus end

static void AnimateActionBuild(CUnit &unit)
{
	//Wyrmgus start
//	CAnimations *animations = unit.Type->Animations;
	CAnimations *animations = unit.GetAnimations();
	//Wyrmgus end

	if (animations == NULL) {
		return ;
	}
	if (animations->Build) {
		UnitShowAnimation(unit, animations->Build);
	} else if (animations->Repair) {
		UnitShowAnimation(unit, animations->Repair);
	}
}


/**
**  Build the building
**
**  @param unit  worker which build.
*/
bool COrder_Build::BuildFromOutside(CUnit &unit) const
{
	AnimateActionBuild(unit);

	if (this->BuildingUnit == NULL) {
		return false;
	}

	if (this->BuildingUnit->CurrentAction() == UnitActionBuilt) {
		COrder_Built &targetOrder = *static_cast<COrder_Built *>(this->BuildingUnit->CurrentOrder());
		CUnit &goal = *const_cast<COrder_Build *>(this)->BuildingUnit;


		targetOrder.ProgressHp(goal, 100);
	}
	if (unit.Anim.Unbreakable) {
		return false;
	}
	return this->BuildingUnit->CurrentAction() != UnitActionBuilt;
}


/* virtual */ void COrder_Build::Execute(CUnit &unit)
{
	if (unit.Wait) {
		if (!unit.Waiting) {
			unit.Waiting = 1;
			unit.WaitBackup = unit.Anim;
		}
		//Wyrmgus start
//		UnitShowAnimation(unit, unit.Type->Animations->Still);
		UnitShowAnimation(unit, unit.GetAnimations()->Still);
		//Wyrmgus end
		unit.Wait--;
		return;
	}
	if (unit.Waiting) {
		unit.Anim = unit.WaitBackup;
		unit.Waiting = 0;
	}
	if (this->State <= State_MoveToLocationMax) {
		if (this->MoveToLocation(unit)) {
			this->Finished = true;
			return ;
		}
	}
	const CUnitType &type = this->GetUnitType();

	if (State_NearOfLocation <= this->State && this->State < State_StartBuilding_Failed) {
		//Wyrmgus start
		/*
		if (CheckLimit(unit, type) == false) {
			this->Finished = true;
			return ;
		}
		*/
		//Wyrmgus end
		CUnit *ontop = this->CheckCanBuild(unit);

		if (ontop != NULL) {
			//Wyrmgus start
			if (CheckLimit(unit, type) == false) {
				this->Finished = true;
				return ;
			}
			//Wyrmgus end
			this->StartBuilding(unit, *ontop);
		}
		else { /* can't be built */
			// Check if already building
			const Vec2i pos = this->goalPos;
			const CUnitType &type = this->GetUnitType();

			//Wyrmgus start
//			CUnit *building = AlreadyBuildingFinder(unit, type).Find(Map.Field(pos));
			CUnit *building = AlreadyBuildingFinder(unit, type).Find(Map.Field(pos, this->MapLayer));
			//Wyrmgus end

			if (building != NULL) {
				this->HelpBuild(unit, *building);
				// HelpBuild replaces this command so return immediately
				return ;
			}

			// failed, retry later
			this->State++;
			// To keep the load low, retry each 10 cycles
			// NOTE: we can already inform the AI about this problem?
			unit.Wait = 10;
		}
	}
	if (this->State == State_StartBuilding_Failed) {
		unit.Player->Notify(NotifyYellow, unit.tilePos,
							//Wyrmgus start
							unit.MapLayer,
//							_("You cannot build %s here"), type.Name.c_str());
							_("You cannot build %s here"), type.GetDefaultName(*unit.Player).c_str());
							//Wyrmgus end
		if (unit.Player->AiEnabled) {
			AiCanNotBuild(unit, type);
		}
		this->Finished = true;
		return ;
	}
	if (this->State == State_BuildFromOutside) {
		//Wyrmgus start
//		if (this->BuildFromOutside(unit)) {
//			this->Finished = true;
//		}
		if (this->BuildingUnit != NULL) {
			this->HelpBuild(unit, *this->BuildingUnit);
		} else {
			this->Finished = true;
		}
		//Wyrmgus end
	}
}

//@}
