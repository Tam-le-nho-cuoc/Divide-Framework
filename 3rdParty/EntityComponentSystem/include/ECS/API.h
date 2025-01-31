///-------------------------------------------------------------------------------------------------
/// File:	include\API.h.
///
/// Summary:	API.
/// 

/*
Preprocessor defines:

	ECS_DISABLE_LOGGING			- Disable logging feature.



*/

#pragma once
#ifndef __ECS_API_H__
#define __ECS_API_H__

#define ENITY_LUT_GROW						1024

#define ENITY_T_CHUNK_SIZE					512

#define COMPONENT_LUT_GROW					1024

#define COMPONENT_T_CHUNK_SIZE				512

// 4MB 
#define ECS_EVENT_MEMORY_BUFFER_SIZE		4194304

// 8MB
#define ECS_SYSTEM_MEMORY_BUFFER_SIZE		8388608


#include "Platform.h"
#include "Log/Logger.h"

namespace ECS 
{
	namespace Log {

		namespace Internal
		{
#if !defined(ECS_DISABLE_LOGGING)

			class  LoggerManager;
			extern LoggerManager*				ECSLoggerManager;

			///-------------------------------------------------------------------------------------------------
			/// Fn:	ECS_API Log::Logger* GetLogger(const char* logger);
			///
			/// Summary:	Returns a log4cpp managed logger instance.
			///
			/// Author:	Tobias Stein
			///
			/// Date:	23/09/2017
			///
			/// Parameters:
			/// logger - 	The logger.
			///
			/// Returns:	Null if it fails, else the logger.
			///-------------------------------------------------------------------------------------------------

			ECS_API Log::Logger* GetLogger(const char* logger);
#endif
		}
	}

	namespace Memory
	{
		namespace Internal
		{
			class  MemoryManager;
			extern MemoryManager*				ECSMemoryManager;
		}
	}

	namespace Event
	{
		class EventHandler;
	}


	class EntityManager;
	class SystemManager;
	class ComponentManager;



	namespace Memory
	{
		///-------------------------------------------------------------------------------------------------
		/// Class:	GlobalMemoryUser
		///
		/// Summary:	Any class that wants to use the global memory must derive from this class.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	9/09/2017
		///-------------------------------------------------------------------------------------------------

		class ECS_API GlobalMemoryUser
		{
		private:

			Internal::MemoryManager* ECS_MEMORY_MANAGER;

		public:

			GlobalMemoryUser();
			virtual ~GlobalMemoryUser();

			const void* Allocate(size_t memSize, const char* user = nullptr);
			void Free(void* pMem);
		};

	} // namespace ECS::Memory


	ECS_API void					Initialize();
	ECS_API void					Terminate();

} // namespace ECS

#endif // __ECS_API_H__