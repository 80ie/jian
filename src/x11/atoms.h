#pragma once

#include <xcb/xcb.h>

#include <optional>

// The atoms the panel reads and sends. WM_NAME and WM_CLASS are predefined.
struct Atoms {
    xcb_atom_t clientList = XCB_ATOM_NONE;
    xcb_atom_t clientListStacking = XCB_ATOM_NONE;
    xcb_atom_t activeWindow = XCB_ATOM_NONE;
    xcb_atom_t currentDesktop = XCB_ATOM_NONE;
    xcb_atom_t closeWindow = XCB_ATOM_NONE;
    xcb_atom_t wmName = XCB_ATOM_NONE;
    xcb_atom_t wmIcon = XCB_ATOM_NONE;
    xcb_atom_t wmDesktop = XCB_ATOM_NONE;
    xcb_atom_t wmState = XCB_ATOM_NONE;
    xcb_atom_t stateHidden = XCB_ATOM_NONE;
    xcb_atom_t stateSticky = XCB_ATOM_NONE;
    xcb_atom_t stateSkipTaskbar = XCB_ATOM_NONE;
    xcb_atom_t stateMaximizedVert = XCB_ATOM_NONE;
    xcb_atom_t stateMaximizedHorz = XCB_ATOM_NONE;
    xcb_atom_t wmChangeState = XCB_ATOM_NONE;
    xcb_atom_t utf8String = XCB_ATOM_NONE;
};

// Interns them all in one round trip.
std::optional<Atoms> internAtoms(xcb_connection_t* connection);
