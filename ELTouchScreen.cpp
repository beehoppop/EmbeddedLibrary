
#include <ELModule.h>
#include <ELTouchScreen.h>

CModule_TouchScreen	CModule_TouchScreen::module;
CModule_TouchScreen*	gTouchScreen;

CModule_TouchScreen::CModule_TouchScreen(
	)
{

}

void
CModule_TouchScreen::SetTouchScreenProvider(
	ITouchScreenHandler*	inProvider)
{

}

void
CModule_TouchScreen::RegisterEventHandler(
	STouchScreenRect const&		inRect,
	ITouchScreenHandler*		inHandler,
	TTouchScreenHandlerMethod	inMethod,
	void*						inRefCont)
{

}

void
CModule_TouchScreen::Setup(
	void)
{

}

void
CModule_TouchScreen::Update(
	uint32_t	inDeltaTimeUS)
{

}

