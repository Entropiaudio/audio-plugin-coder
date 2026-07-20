#include "../../SupressMSVCWarnings.h"

#if INCLUDE_MOONBASE_UI
#include "LAFs.cpp"
#include "ErrorDisplay.cpp"
#include "Content/Includes.cpp"
#include "ActivationComponent.cpp"
#include "UI_Impl.cpp"
#endif

// always including the UpdateBadge component, even if the Activation UI is not included, 
// so users can still add the badge easily if they don't use the full Activation UI

#include "UpdateBadge.cpp"