/*
** GLIM - OpenGL Immediate Mode
** Copyright Jan Krassnigg (Jan@Krassnigg.de)
** For more details, see the included Readme.txt.
*/

#ifndef GLIM_DECLARATIONS_H
#define GLIM_DECLARATIONS_H

#ifndef AE_RENDERAPI_OPENGL
#define AE_RENDERAPI_OPENGL
#endif

#include "Core/TemplateLibraries/Headers/String.h"
#include <assert.h>

namespace NS_GLIM
{

    //! The enum holding all important GLIM configuration values.
    enum class GLIM_ENUM : int
    {
        GLIM_NOPRIMITIVE,        // for internal use

        GLIM_TRIANGLES,            //!< Can be passed to GLIM::Begin
        GLIM_TRIANGLE_STRIP,    //!< Can be passed to GLIM::Begin    (not yet implemented)
        GLIM_TRIANGLE_FAN,        //!< Can be passed to GLIM::Begin
        GLIM_QUADS,                //!< Can be passed to GLIM::Begin
        GLIM_QUAD_STRIP,        //!< Can be passed to GLIM::Begin    (not yet implemented)
        GLIM_POINTS,            //!< Can be passed to GLIM::Begin
        GLIM_LINES,                //!< Can be passed to GLIM::Begin
        GLIM_LINE_STRIP,        //!< Can be passed to GLIM::Begin
        GLIM_LINE_LOOP,            //!< Can be passed to GLIM::Begin
        GLIM_POLYGON,            //!< Can be passed to GLIM::Begin

        GLIM_NODATA,            // for internal use
        GLIM_1F,                // for internal use
        GLIM_2F,                // for internal use
        GLIM_3F,                // for internal use
        GLIM_4F,                // for internal use
        GLIM_1I,                // for internal use
        GLIM_2I,                // for internal use
        GLIM_3I,                // for internal use
        GLIM_4I,                // for internal use
        GLIM_4UB,                // for internal use
    };

    enum class GLIM_API : unsigned int
    {
        GLIM_NONE,
        GLIM_OPENGL,
        GLIM_D3D11,
    };

    // Base class for GLIM-objects. Defines the interface.
    class GLIM_Interface;

    // One implementation of GLIM_Interface.
    class GLIM_BATCH;

    // One global GLIM_BATCH is always defined for immediate use.
    extern GLIM_BATCH glim;

    struct GlimArrayData;

    typedef GlimArrayData& GLIM_ATTRIBUTE;


    //! Declaration for a callback-function that will be called directly before each drawcall / shader-query.
    typedef void (*GLIM_CALLBACK)(void);

#ifdef AE_RENDERAPI_D3D11
    typedef void (*GLIM_CALLBACK_SETINPUTLAYOUT)(GLIM_Interface* pBatch, const vector<D3D11_INPUT_ELEMENT_DESC>& Signature);
    typedef void (*GLIM_CALLBACK_RELEASERESOURCE)(ID3D11Resource* pResource);
#endif

    //! Assert Macro used internally.
    inline void GLIM_CHECK (bool bCondition, [[maybe_unused]] const char* szErrorMsg) noexcept
    {
        if (bCondition)
            return;

        assert(false && szErrorMsg);
    }

}

#pragma once

#endif


