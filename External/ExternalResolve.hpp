#pragma once

// Core
#include "Core/UnityExternalMemory.hpp"
#include "Core/UnityExternalMemoryConfig.hpp"
#include "Core/UnityExternalTypes.hpp"

// Default memory reader (WinAPI implementation).
// Users can implement their own IMemoryAccessor and call SetGlobalMemoryAccessor
// to plug in driver-based or custom memory backends.
#include "MemoryRead/UnityExternalMemoryWinAPI.hpp"

// GameObjectManager
#include "GameObjectManager/UnityExternalGOM.hpp"
#include "GameObjectManager/Managed/ManagedObject.hpp"
#include "GameObjectManager/Native/NativeComponent.hpp"
#include "GameObjectManager/Native/NativeGameObject.hpp"
#include "GameObjectManager/Native/NativeTransform.hpp"

// Camera
#include "Camera/UnityExternalCamera.hpp"
#include "Camera/UnityExternalWorldToScreen.hpp"
