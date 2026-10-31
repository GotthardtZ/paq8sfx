#pragma once

/**
 * A static (common) 1024-element lookup table for integer division.
 * Every call fills the table again, but the table is created only once.
 */
int* DivisionTable_getDT(void);
