/**
 * Author: luoqi
 * Created Date: 2025-10-09 16:10:25
 * Last Modified: 2026-03-31 11:21:41
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2025 <*****>
 * Description:
 */

#pragma once
#ifndef _AUTOCRLF_HPP_
#define _AUTOCRLF_HPP_

#include <streambuf>
#include <iostream>

class AutoCRLF : public std::streambuf {
public:
    AutoCRLF() 
    {
        orgbuf = std::cout.rdbuf();  // save original buffer
        std::cout.rdbuf(this);       // set current object as cout's new buffer
    }

    ~AutoCRLF() 
    {
        std::cout.rdbuf(orgbuf);     // restore original buffer
    }

    virtual int_type overflow(int_type c) override
    {
        if(c != traits_type::eof()) {
            if(c == '\n') {
                if(orgbuf->sputc('\r') == traits_type::eof())  // output \r
                    return traits_type::eof();
            }
            return orgbuf->sputc(c);  // output c
        }
        return traits_type::not_eof(c);
    }
    
private:
    std::streambuf *orgbuf;  // save original buffer
};

#endif
