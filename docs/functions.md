# DuckDB Point Cloud Extension Function Reference

## Function Index 
**[Scalar Functions](#scalar-functions)**

| Function | Summary |
| --- | --- |

**[Aggregate Functions](#aggregate-functions)**

| Function | Summary |
| --- | --- |

**[Table Functions](#table-functions)**

| Function | Summary |
| --- | --- |
| [`PDAL_Drivers`](#pdal_drivers) |  |

----

## Scalar Functions

## Aggregate Functions

## Table Functions

### PDAL_Drivers

#### Signature

```sql
PDAL_Drivers ()
```

#### Description


		Returns the list of supported stage types of a PDAL Pipeline.

		The stages of a PDAL Pipeline are divided into Readers, Filters and Writers: https://pdal.io/en/stable/stages/stages.html
	

#### Example

```sql

		SELECT * FROM PDAL_Drivers();

		┌─────────────────────────────┬─────────────────────────────────────────────────────────────────────────────────┐
		│            name             │                                     description                                 │
		│           varchar           │                                       varchar                                   │
		├─────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
		│ filters.approximatecoplanar │ Estimates the planarity of a neighborhood of points using eigenvalues.          │
		│ filters.assign              │ Assign values for a dimension range to a specified value.                       │
		│ filters.chipper             │ Organize points into spatially contiguous, squarish, and non-overlapping chips. │
		│ filters.cluster             │ Extract and label clusters using Euclidean distance.                            │
		│      ·                      │      ·                                                                          │
		│      ·                      │      ·                                                                          │
		│      ·                      │      ·                                                                          │
		│ readers.slpk                │ SLPK Reader                                                                     │
		│ readers.smrmsg              │ SBET smrmsg Reader                                                              │
		│ readers.stac                │ STAC Reader                                                                     │
		│ readers.terrasolid          │ TerraSolid Reader                                                               │
		│ writers.copc                │ COPC Writer                                                                     │
		│ writers.gdal                │ Write a point cloud as a GDAL raster.                                           │
		│ writers.las                 │ ASPRS LAS 1.0 - 1.4 writer                                                      │
		│ writers.text                │ Text Writer                                                                     │
		├─────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────┤
		│ 119 rows                                                                                            2 columns │
		└───────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
	
```

----

