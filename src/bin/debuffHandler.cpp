#include "include/debuffHandler.h"
#include "include/data.h"
#include "include/settings.h"
#include "include/offsets.h"
#include "include/Utils.h"
#include "valhallaCombat.hpp"
using uniqueLocker = std::unique_lock<std::shared_mutex>;
using sharedLocker = std::shared_lock<std::shared_mutex>;
/*Called every frame.
Iterate through the set of actors debuffing.
Check the actors' stamina. If the actor's stamina has fully recovered, remove the actor from the set.
Check the actor's UI counter, if the counter is less than 0, flash the actor's UI.*/
void debuffHandler::update() {

	uniqueLocker lock(mtx_actorInDebuff);
	if (actorInDebuff.empty()) {
		ValhallaCombat::GetSingleton()->deactivateUpdate(ValhallaCombat::debuffHandler);
	}
	auto it = actorInDebuff.begin();
	while (it != actorInDebuff.end()) {
		auto handle = *it;
		if (!handle) {
			it = actorInDebuff.erase(it);  //erase actor from debuff set.
			continue;
		}
		auto actor = handle.get().get();
		if (!actor) {
			it = actorInDebuff.erase(it);  //erase actor from debuff set.
			continue;
		}
		if (actor->GetActorValue(RE::ActorValue::kStamina) >= 
			actor->GetPermanentActorValue(RE::ActorValue::kStamina) 
			+ actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina)) { //offset max stamina based on modifier
			revertDebuffUI(actor);
			it = actorInDebuff.erase(it);
			continue;
		}
		++it;
	}	
}


void debuffHandler::initStaminaDebuff(RE::Actor* a_actor) {	
	if (!settings::bStaminaDebuffToggle) {
		return;
	}
	RE::ActorHandle handle = a_actor->GetHandle();
	{
		uniqueLocker lock(mtx_actorInDebuff);
		auto it = actorInDebuff.find(handle);
		if (it != actorInDebuff.end()) {
			return;
		}
		actorInDebuff.insert(handle);	
	}

	ValhallaCombat::GetSingleton()->activateUpdate(ValhallaCombat::HANDLER::debuffHandler);
}

void debuffHandler::initDebuffUI(RE::Actor* a_actor) {
	if (!settings::bUIAlert) {
		return;
	}
	TrueHUDUtils::greyoutAvMeter(a_actor, RE::ActorValue::kStamina);
	if (a_actor->IsPlayerRef() && !async_pcStaminaMeterFlash_b) {
		std::jthread t(async_pcStaminaMeterFlash);
		t.detach();
		async_pcStaminaMeterFlash_b = true;
	}
}


void debuffHandler::revertDebuffUI(RE::Actor* a_actor) {
	if (!settings::bUIAlert) {
		return;
	}
	if (a_actor->IsPlayerRef()) {
		async_pcStaminaMeterFlash_b = false;
	}
	TrueHUDUtils::revertAvMeter(a_actor, RE::ActorValue::kStamina);
}

void debuffHandler::stopDebuff(RE::Actor* a_actor) {
	auto handle = a_actor->GetHandle();
	uniqueLocker lock(mtx_actorInDebuff);
	auto it = actorInDebuff.find(handle);
	if (it == actorInDebuff.end()) {
		return;
	}
	actorInDebuff.erase(it);
	revertDebuffUI(a_actor);
}

	

bool debuffHandler::isInDebuff(RE::Actor* a_actor) {
	sharedLocker lock(mtx_actorInDebuff);
	auto handle = a_actor->GetHandle();
	return actorInDebuff.contains(handle);
} 


void debuffHandler::async_pcStaminaMeterFlash() {
	while (true) {
		if (async_pcStaminaMeterFlash_b) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			TrueHUDUtils::flashActorValue(RE::PlayerCharacter::GetSingleton(), RE::ActorValue::kStamina);
		}
		else {
			return;
		}
	}
}
