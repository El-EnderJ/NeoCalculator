/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// -*- mode:C++ ; compile-command: "g++ -I.. -g -c myostream.h" -*-
/*
*  Copyright (C) 2011 B. Parisse, 12 r. Conrad Kilian, 38950 St Martin le Vinoux
*  Copyright (C) 2013 J-Y. Avenard, somewhere in the world
*/

#ifndef MYOSTREAM_H
#define MYOSTREAM_H

#include <ostream>
#include "iostream"

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  class my_ostream: public std::ostream {
  public:
    template <typename T>
    friend my_ostream & operator<<(my_ostream &, const T &);

    // Additional overload to handle ostream specific io manipulators 
    friend my_ostream & operator<<(my_ostream &, std::ostream & (*)(std::ostream &));

    //my_ostream do not own ostream* os... Caller must handle delete
    my_ostream(std::ostream * os) : std::ostream(os->rdbuf()) { }
    my_ostream(std::streambuf * sb) : std::ostream(sb) { }
    virtual ~my_ostream() { }
  };

  template <typename T>
  inline my_ostream & operator<<(my_ostream & out, const T & value){
    static_cast<std::ostream &>(out) << value;
    return out;
  }

  //  overload for double
  template <>
  inline my_ostream & operator<<(my_ostream & out, const double & value){
    char buf[50];
    my_sprintf(buf, "%f", value);
    return out << buf;
  }

  //  overload for float
  template <>
  inline my_ostream & operator<<(my_ostream & out, const float & value){
    char buf[50];
    my_sprintf(buf, "%f", value);
    return out << buf;
  }

  //  overload for int
  template <>
  inline my_ostream & operator<<(my_ostream & out, const int & value){
    char buf[50];
    my_sprintf(buf, "%d", value);
    return out << buf;
  }

  //  overload for unsigned int
  template <>
  inline my_ostream & operator<<(my_ostream & out, const unsigned int & value){
    char buf[50];
    my_sprintf(buf, "%u", value);
    return out << buf;
  }

  //  overload for std::ostream specific io manipulators
  inline my_ostream & operator<<(my_ostream & out, std::ostream & (*func)(std::ostream &)){
    static_cast<std::ostream &>(out) << func;
    return out;
  }

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

#endif // MYOSTREAM_H
