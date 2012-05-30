#pragma once
#ifndef DISALLOW_COPY_AND_ASSIGN_H
#define DISALLOW_COPY_AND_ASSIGN_H

// From Google Style Guide
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)
#endif

#endif //DISALLOW_COPY_AND_ASSIGN_H