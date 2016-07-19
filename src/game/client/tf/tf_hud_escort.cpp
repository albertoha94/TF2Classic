//=============================================================================
//
// Purpose: Payload HUD
//
//=============================================================================
#include "cbase.h"
#include "tf_hud_escort.h"
#include "tf_hud_freezepanel.h"
#include "c_team_objectiveresource.h"

using namespace vgui;

CTFHudEscort::CTFHudEscort( Panel *pParent, const char *pszName ) : EditablePanel( pParent, pszName )
{
	m_pLevelBar = new ImagePanel( this, "LevelBar" );

	m_pEscortItemPanel = new EditablePanel( this, "EscortItemPanel" );
	m_pCapNumPlayers = new CExLabel( m_pEscortItemPanel, "CapNumPlayers", "x0" );
	m_pRecedeTime = new CExLabel( m_pEscortItemPanel, "RecedeTime", "0" );
	m_pCapPlayerImage = new ImagePanel( m_pEscortItemPanel, "CapPlayerImage" );
	m_pBackwardsImage = new ImagePanel( m_pEscortItemPanel, "Speed_Backwards" );
	m_pBlockedImage = new ImagePanel( m_pEscortItemPanel, "Blocked" );

	for ( int i = 0; i < MAX_CONTROL_POINTS; i++ )
	{
		m_pCPImages[i] = new ImagePanel( this, VarArgs( "cp_%d", i ) );
	}

	m_pCPImageTemplate = new ImagePanel( this, "SimpleControlPointTemplate" );

	m_flProgress = -1.0f;
	m_flRecedeTime = 0.0f;

	m_iTeamNum = TF_TEAM_BLUE;

	ivgui()->AddTickSignal( GetVPanel() );

	ListenForGameEvent( "escort_progress" );
	ListenForGameEvent( "escort_speed" );
	ListenForGameEvent( "escort_recede" );
	ListenForGameEvent( "controlpoint_initialized" );
	ListenForGameEvent( "controlpoint_updateimages" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudEscort::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Setup conditions.
	KeyValues *pConditions = NULL;
	if ( m_iTeamNum >= FIRST_GAME_TEAM )
	{
		pConditions = new KeyValues( "conditions" );

		KeyValues *pTeamKey = new KeyValues( m_iTeamNum == TF_TEAM_RED ? "if_team_red" : "if_team_blue" );
		pConditions->AddSubKey( pTeamKey );
	}

	LoadControlSettings( "Resource/UI/ObjectiveStatusEscort.res", NULL, NULL, pConditions );

	if ( pConditions )
		pConditions->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudEscort::OnChildSettingsApplied( KeyValues *pInResourceData, Panel *pChild )
{
	// Apply settings from template to all CP icons.
	if ( pChild == m_pCPImageTemplate )
	{
		for ( int i = 0; i < MAX_CONTROL_POINTS; i++ )
		{
			if ( m_pCPImages[i] )
			{
				m_pCPImages[i]->ApplySettings( pInResourceData );
			}
		}
	}

	BaseClass::OnChildSettingsApplied( pInResourceData, pChild );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFHudEscort::IsVisible( void )
{
	if ( IsInFreezeCam() )
		return false;

	return BaseClass::IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudEscort::FireGameEvent( IGameEvent *event )
{
	if ( V_strcmp( event->GetName(), "controlpoint_initialized" ) == 0 )
	{
		UpdateCPImages( true, -1 );
		return;
	}

	if ( V_strcmp( event->GetName(), "controlpoint_updateimages" ) == 0 )
	{
		UpdateCPImages( false, event->GetInt( "index" ) );
		return;
	}

	// Ignore events not related to the watched team.
	if ( event->GetInt( "team" ) != m_iTeamNum )
		return;

	if ( V_strcmp( event->GetName(), "escort_progress" ) == 0 )
	{
		if ( event->GetBool( "reset" ) )
		{
			m_flProgress = 0.0f;
		}
		else
		{
			m_flProgress = event->GetFloat( "progress" );
		}
	}
	else if ( V_strcmp( event->GetName(), "escort_speed" ) == 0 )
	{
		// Get the number of cappers.
		int iNumCappers = event->GetInt( "players" );
		int iSpeedLevel = event->GetInt( "speed" );

		if ( m_pEscortItemPanel )
		{
			m_pEscortItemPanel->SetDialogVariable( "numcappers", iNumCappers );
		}

		// Show the number and icon if there any cappers present.
		bool bShowCappers = ( iNumCappers > 0 );
		if ( m_pCapNumPlayers )
		{
			m_pCapNumPlayers->SetVisible( bShowCappers );
		}
		if ( m_pCapPlayerImage )
		{
			m_pCapPlayerImage->SetVisible( bShowCappers );
		}

		// -1 cappers means the cart is blocked.
		if ( m_pBlockedImage )
		{
			m_pBlockedImage->SetVisible( iNumCappers == -1 );
		}

		// -1 speed level means the cart is receding.
		if ( m_pBackwardsImage )
		{
			// NO! CART MOVES WRONG VAY!
			m_pBackwardsImage->SetVisible( iSpeedLevel == -1 );
		}
	}
	else if ( V_strcmp( event->GetName(), "escort_recede" ) == 0 )
	{
		// Get the current recede time of the cart.
		m_flRecedeTime = event->GetFloat( "recedetime" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudEscort::OnTick( void )
{
	if ( !IsVisible() )
		return;

	if ( m_pEscortItemPanel && m_pLevelBar )
	{
		// Position the cart icon so the arrow points at its position on the track.
		int x, y, wide, tall, pos;
		m_pLevelBar->GetBounds( x, y, wide, tall );

		pos = Lerp( m_flProgress, x, x + wide ) - m_pEscortItemPanel->GetWide() / 2;

		m_pEscortItemPanel->SetPos( pos, m_pEscortItemPanel->GetYPos() );
	}

	// Calculate time left until receding.
	float flRecedeTimeLeft = ( m_flRecedeTime != 0.0f ) ? m_flRecedeTime - gpGlobals->curtime : 0.0f;

	if ( m_pEscortItemPanel )
	{
		m_pEscortItemPanel->SetDialogVariable( "recede", (int)ceil( flRecedeTimeLeft ) );
	}

	if ( m_pRecedeTime )
	{
		// Show the timer if the cart is close to starting to recede.
		bool bShow = flRecedeTimeLeft > 0 && flRecedeTimeLeft < 30.0f;
		if ( m_pRecedeTime->IsVisible() != bShow )
		{
			m_pRecedeTime->SetVisible( bShow );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudEscort::UpdateCPImages( bool bUpdatePositions, int iIndex )
{
	if ( !ObjectiveResource() )
		return;

	for ( int i = 0; i < MAX_CONTROL_POINTS; i++ )
	{
		// If an index is specified only update the specified point.
		if ( iIndex != -1 && i != iIndex )
			continue;

		ImagePanel *pImage = m_pCPImages[i];
		if ( !pImage )
			continue;

		if ( bUpdatePositions )
		{
			// Check if this point exists and should be shown.
			if ( i >= ObjectiveResource()->GetNumControlPoints() ||
				( ObjectiveResource()->PlayingMiniRounds() && ObjectiveResource()->IsCPVisible( i ) == false ) )
			{
				pImage->SetVisible( false );
				continue;
			}

			if ( !pImage->IsVisible() )
				pImage->SetVisible( true );

			// Get the control point position.
			float flDist = ObjectiveResource()->GetPathDistance( i );

			if ( m_pLevelBar )
			{
				int x, y, wide, tall, pos;
				m_pLevelBar->GetBounds( x, y, wide, tall );

				pos = Lerp( flDist, x, x + wide ) - pImage->GetWide() / 2;

				pImage->SetPos( pos, pImage->GetYPos() );
			}
		}

		// Set the icon according to team.
		const char *pszImage = NULL;
		switch ( ObjectiveResource()->GetOwningTeam( i ) )
		{
		case TF_TEAM_RED:
			pszImage = "../hud/cart_point_red";
			break;
		case TF_TEAM_BLUE:
			pszImage = "../hud/cart_point_blue";
			break;
		default:
			pszImage = "../hud/cart_point_neutral";
			break;
		}

		pImage->SetImage( pszImage );
	}
}
