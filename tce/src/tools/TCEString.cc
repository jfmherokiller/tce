/*
    Copyright (c) 2002-2009 Tampere University of Technology.

    This file is part of TTA-Based Codesign Environment (TCE).

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */
/**
 * @file TCEString.cc
 *
 * Definition of TCEString class.
 *
 * @author Viljami Korhonen 2008 (viljami.korhonen-no.spam-tut.fi)
 * @note rating: red
 */
#include <string>
#include "TCEString.hh"

TCEString::TCEString() : std::string() {
}

TCEString::TCEString(const char* text) : std::string(text) {
}

TCEString::TCEString(const std::string& text) : std::string(text) {
}
TCEString::~TCEString() {
}

/**
 * Returns true if two strings are equal if the case is not taken into account.
 *
 * @param a A string.
 * @param b Another string.
 * @return True if strings equal case-insensitively.
 */
bool
TCEString::ciEqual(const TCEString& other) const {
    return (lower() == other.lower());
}

TCEString
TCEString::lower() const {
    return StringTools::stringToLower(*this);
}


TCEString
TCEString::upper() const {
    return StringTools::stringToUpper(*this);
}

