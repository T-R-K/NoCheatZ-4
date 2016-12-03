/*
	Copyright 2012 - Le Padellec Sylvain

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "ConCommandHookListener.h"

#include "plugin.h" // For below
#include "MagicValues.h" // GET_PLUGIN_COMMAND_INDEX
#include "Systems/ConfigManager.h" // ConfigManager
#include "Systems/Logger.h" // DebugMessage
#include "Players/NczPlayerManager.h"
#include "Hooks/temp_HookGuard.h"

ConCommandHookListener::ConCommandListenersListT ConCommandHookListener::m_listeners;

ConCommandHookListener::ConCommandHookListener ()
{
	HookGuard<ConCommandHookListener>::Required ();
}

ConCommandHookListener::~ConCommandHookListener ()
{
	if( HookGuard<ConCommandHookListener>::IsCreated() )
	{
		HookGuard<ConCommandHookListener>::GetInstance ()->UnhookAll ();
		HookGuard<ConCommandHookListener>::DestroyInstance ();
	}
}

void ConCommandHookListener::HookDispatch ( void* cmd )
{
	HookInfo info ( cmd, ConfigManager::GetInstance ()->vfid_dispatch, ( DWORD ) RT_nDispatch );
	HookGuard<ConCommandHookListener>::GetInstance ()->VirtualTableHook ( info );
}

#ifdef GNUC
void HOOKFN_INT ConCommandHookListener::RT_nDispatch ( void* cmd, SourceSdk::CCommand const &args )
#else
void HOOKFN_INT ConCommandHookListener::RT_nDispatch ( void* cmd, void*, SourceSdk::CCommand const &args )
#endif
{
	DebugMessage ( Helpers::format ( "RT_nDispatch : %s\n", SourceSdk::InterfacesProxy::ConCommand_GetName ( cmd ) ) );

	const int index ( GET_PLUGIN_COMMAND_INDEX () );
	bool bypass ( false );
	if( index >= PLUGIN_MIN_COMMAND_INDEX && index <= PLUGIN_MAX_COMMAND_INDEX )
	{
		if( index <= NczPlayerManager::GetInstance ()->GetMaxIndex () ) // https://github.com/L-EARN/NoCheatZ-4/issues/59#issuecomment-230506264
		{
			PlayerHandler::const_iterator ph = NczPlayerManager::GetInstance ()->GetPlayerHandlerByIndex ( index );

			if( ph > SlotStatus::INVALID )
			{
				DebugMessage( Helpers::format ( "Testing ConCommand %s of %s\n", SourceSdk::InterfacesProxy::ConCommand_GetName ( cmd ), ph->GetName () ) );

				ConCommandListenersListT::elem_t* it ( m_listeners.GetFirst () );
				while( it != nullptr )
				{
					int const c ( it->m_value.listener->m_mycommands.Find ( cmd ) );
					if( c != -1 )
					{
						bypass |= it->m_value.listener->RT_ConCommandCallback ( ph, cmd, args );
					}
					it = it->m_next;
				}
			}
			else
			{
				bypass = true;
			}
		}
		else
		{
			bypass = true;
		}
	}
	else if( index == 0 )
	{
		ConCommandListenersListT::elem_t* it = m_listeners.GetFirst ();
		while( it != nullptr )
		{
			int const c = it->m_value.listener->m_mycommands.Find ( cmd );
			if( c != -1 )
			{
				bypass |= it->m_value.listener->RT_ConCommandCallback ( PlayerHandler::end (), cmd, args );
			}
			it = it->m_next;
		}
	}
	else
	{
		bypass = true;
	}

	if( !bypass )
	{
		//Assert(it != nullptr);

		ST_W_STATIC Dispatch_t gpOldFn;
		*( DWORD* )&( gpOldFn ) = HookGuard<ConCommandHookListener>::GetInstance ()->RT_GetOldFunction ( cmd, ConfigManager::GetInstance ()->vfid_dispatch );
		gpOldFn ( cmd, args );
	}
	else
	{
		DebugMessage ( Helpers::format ( "Bypassed ConCommand %s of %d\n", SourceSdk::InterfacesProxy::ConCommand_GetName ( cmd ), index ) );
	}
}

void ConCommandHookListener::RegisterConCommandHookListener ( ConCommandHookListener const * const listener )
{
	LoggerAssert ( !listener->m_mycommands.IsEmpty () );

	ConCommandListenersListT::elem_t* it ( m_listeners.FindByListener ( listener ) );
	if( it == nullptr )
	{
		m_listeners.Add ( listener );
	}

	size_t cmd_pos ( 0 );
	size_t const max_pos ( listener->m_mycommands.Size () );
	do
	{
		HookDispatch ( listener->m_mycommands[ cmd_pos ] );
	}
	while( ++cmd_pos != max_pos );
}

void ConCommandHookListener::RemoveConCommandHookListener ( ConCommandHookListener * const listener )
{
	listener->m_mycommands.RemoveAll ();
	m_listeners.Remove ( listener );
}

