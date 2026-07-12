#pragma once

#include "Harddisk.h"


void LoadConfiguration(bool loadImages);
void InsertHardDisks(const UINT slot, LPCSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot);
void GetAppleWindowTitle();

void CtrlReset();
void ResetMachineState();
