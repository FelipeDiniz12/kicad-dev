/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * 2017 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file profile.h:
 * @brief Simple profiling functions for measuring code execution time.
 */

#ifndef __TPROFILE_H
#define __TPROFILE_H

#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>

#include <cstdio>
#include <string>

/**
 * The class PROF_COUNTER is a small class to help profiling.
 * It allows the calculation of the elapsed time (in millisecondes) between
 * its creation (or the last call to Start() ) and the last call to Stop()
 */
class PROF_COUNTER
{
public:
    /**
     * Creates a PROF_COUNTER for measuring an elapsed time in milliseconds
     * @param aName = a string that will be printed in message.
     * @param aAutostart = true (default) to immediately start the timer
     */
    PROF_COUNTER( const std::string& aName, bool aAutostart = true ) :
        m_name( aName ), m_running( false )
    {
        if( aAutostart )
            Start();
    }

    /**
     * Creates a PROF_COUNTER for measuring an elapsed time in milliseconds
     * The counter is started and the string to print in message is left empty.
     */
    PROF_COUNTER()
    {
        Start();
    }

    /**
     * Starts or restarts the counter
     */
    void Start()
    {
        m_running = true;
        m_starttime = std::chrono::high_resolution_clock::now();
    }


    /**
     * save the time when this function was called, and set the counter stane to stop
     */
    void Stop()
    {
        if( !m_running )
            return;

        m_stoptime = std::chrono::high_resolution_clock::now();
    }

    /**
     * Print the elapsed time (in ms) to STDERR.
     */
    void Show()
    {
        TIME_POINT display_stoptime = m_running ?
                    std::chrono::high_resolution_clock::now() :
                    m_stoptime;

        std::chrono::duration<double, std::milli> elapsed = display_stoptime - m_starttime;
        std::cerr << m_name << " took " << elapsed.count() << " milliseconds." << std::endl;
    }

    /**
     * @return the elapsed time in ms
     */
    double msecs() const
    {
        TIME_POINT stoptime = m_running ?
                    std::chrono::high_resolution_clock::now() :
                    m_stoptime;

        std::chrono::duration<double, std::milli> elapsed = stoptime - m_starttime;

        return elapsed.count();
    }

private:
    std::string m_name;     // a string printed in message
    bool m_running;

    typedef std::chrono::time_point<std::chrono::high_resolution_clock> TIME_POINT;

    TIME_POINT m_starttime, m_stoptime;
};


/**
 * Function GetRunningMicroSecs
 * An alternate way to calculate an elapset time (in microsecondes) to class PROF_COUNTER
 * @return an ever increasing indication of elapsed microseconds.
 * Use this by computing differences between two calls.
 * @author Dick Hollenbeck
 */
unsigned GetRunningMicroSecs();

class PROF_COUNTER
{
public:
    PROF_COUNTER(const std::string& name, bool autostart = true)
  {
    m_name = name;
    m_running= false;
    if(autostart)
      start();
  }

  void start()
  {
    m_running = true;
    prof_start(&m_cnt);
  }

  void stop()
  {
    if(!m_running)
    return;
    m_running=false;
    prof_end(&m_cnt);
  }

  void show()
  {
    stop();
    fprintf(stderr,"%s took %.1f milliseconds.\n", m_name.c_str(), (double)m_cnt.msecs());
  }
private:
  std::string m_name;
  prof_counter m_cnt;
  bool m_running;
};


#endif
