/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 CERN
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#ifndef __OUTLINE_SHAPE_BUILDER_H
#define __OUTLINE_SHAPE_BUILDER_H

#include <geometry/shape.h>
#include <geometry/shape_line_chain.h>

/**
 * Class OUTLINE_SHAPE_BUILDER
 *
 * Constructs two-segment trace/outline shapes between two defined points.
 */
class OUTLINE_SHAPE_BUILDER
{
public:

    enum SHAPE_TYPE
    {
        SHT_LINE = 0,
        SHT_CORNER_45,
        SHT_CORNER_90,
        SHT_CORNER_ARC_45,
        SHT_CORNER_ARC_90,
        SHT_LAST
    };

    OUTLINE_SHAPE_BUILDER()
    {};

    ~OUTLINE_SHAPE_BUILDER() {};

    void SetArcRadius( int aRadius )
    {
        m_arcRadius = aRadius;
    }

    int GetArcRadius() const
    {
        return m_arcRadius;
    }

    void SetShapeType( SHAPE_TYPE aType )
    {
        m_shapeType = aType;
    }

    SHAPE_TYPE GetShapeType() const
    {
        return m_shapeType;
    }

    void NextShapeType()
    {
        m_shapeType = (SHAPE_TYPE) ( (int) (m_shapeType) + 1 );

        if( m_shapeType == SHT_LAST )
            m_shapeType = SHT_LINE;
    }

    void SetStart( const VECTOR2I& aStart )
    {
        m_start = aStart;
    }

    void SetEnd( const VECTOR2I& aEnd )
    {
        m_end = aEnd;
    }

    void SetArcApproximationFactor( double aFactor )
    {
        m_arcApproxFactor = aFactor;
    }

    const VECTOR2I& GetStart() const
    {
        return m_start;
    }

    const VECTOR2I& GetEnd() const
    {
        return m_end;
    }

    bool IsDiagonal() const
    {
        return m_diagonal;
    }

    void SetDiagonal( bool aDiagonal )
    {
        m_diagonal = aDiagonal;
    }

    void FlipPosture()
    {
        m_diagonal = !m_diagonal;
    }

    bool    Construct( std::vector<SHAPE*>& aShape );
    bool    Construct( std::vector<SHAPE_LINE_CHAIN>& aShape );

private:

    void constructAngledSegs( bool startDiagonal,
            bool is45degree,
            VECTOR2I& a,
            VECTOR2I& b,
            int offset = 0 );

    SHAPE_TYPE m_shapeType = SHT_LINE;
    int m_arcRadius = 2000000;
    bool m_diagonal = false;
    double m_arcApproxFactor = 0.01;
    VECTOR2I m_start, m_end;
};

#endif
