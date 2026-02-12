Release history
---------------

0.4.0
++++++++++++++++++

- All functions use `projection_pushdown` to optimize query execution.
- `PDAL_Read` function supports filter pushdown.

0.3.0
++++++++++++++++++

- New `PDAL_PipelineTable` function to run a PDAL pipeline on an input table.
- Enable support of PDAL to load remote files.

0.2.0
++++++++++++++++++

- Added `dimensions` column to `PDAL_Info` table function. This column provides a list of available fields in each point cloud file.
- For `PDAL_Pipeline` table function, the pipeline can be provided either as a JSON file or as an inline JSON string.

0.1.0
++++++++++++++++++

- First release as DuckDB Community Extension.
