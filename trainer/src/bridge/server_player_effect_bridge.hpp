#pragma once

#include <string>

namespace pztrainer::bridge {

void UpdateServerPlayerEffectBridge();

bool RequestServerTimedMedication(const std::string& id);
bool IsServerTimedMedicationBusy();

bool CanSetServerCharacterStat(const std::string& id);
bool SetServerCharacterStat(const std::string& id, float value);
bool ResetServerCharacterStat(const std::string& id, float default_value);

const std::string& GetServerPlayerEffectStatus();

}  // namespace pztrainer::bridge
