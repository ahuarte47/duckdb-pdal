# DuckDB Point Cloud Extension

## What is this?

This is an extension for DuckDB for manipulating point cloud data using SQL.

The extension is built on top of [PDAL (Point Data Abstraction Library)](https://pdal.io/), a C++ library that enables users to read, write, and process point cloud data and, with this extension, load the data directly into DuckDB using SQL queries.

## How do I get it?

### Loading from community

The DuckDB **Point Cloud Extension** is available as a signed [community extension](https://duckdb.org/community_extensions/list_of_extensions).
See more details on its [DuckDB CE web page](https://duckdb.org/community_extensions/extensions/pdal.html).

To install and load it, you can run the following SQL commands in DuckDB:

```sql
INSTALL pdal FROM community;
LOAD pdal;
```

### Building from source

This extension is based on the [DuckDB extension template](https://github.com/duckdb/extension-template).

## Example Usage

First, make sure to load the extension in your DuckDB session.

Then you can use the extension to read point cloud data from various formats (e.g., LAS, LAZ) and perform transformations on them.

This is the list of available functions:

+ ### PDAL_Drivers

    Returns the list of supported stage types of a PDAL Pipeline.

	The stages of a PDAL Pipeline are divided into Readers, Filters and Writers: https://pdal.io/en/stable/stages/stages.html

    ```sql
    SELECT * FROM PDAL_Drivers();

    ┌─────────────────────────────┬─────────────────────────────────────────────────────────────────────────────────┬───────────┐
    │            name             │                                     description                                 │  category │
    │           varchar           │                                       varchar                                   │  varchar  │
    ├─────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┼───────────┤
    │ filters.approximatecoplanar │ Estimates the planarity of a neighborhood of points using eigenvalues.          │  filters  │
    │ filters.assign              │ Assign values for a dimension range to a specified value.                       │  filters  │
    │ filters.chipper             │ Organize points into spatially contiguous, squarish, and non-overlapping chips. │  filters  │
    │ filters.cluster             │ Extract and label clusters using Euclidean distance.                            │  filters  │
    │ filters.colorinterp         │ Assigns RGB colors based on a dimension and a ramp                              │  filters  │
    │ filters.colorization        │ Fetch and assign RGB color information from a GDAL-readable datasource.         │  filters  │
    │ filters.crop                │ Filter points inside or outside a bounding box or a polygon                     │  filters  │
    │ filters.csf                 │ Cloth Simulation Filter (Zhang et al., 2016)                                    │  filters  │
    │ filters.dbscan              │ DBSCAN Clustering.                                                              │  filters  │
    │ filters.decimation          │ Rank decimation filter. Keep every Nth point                                    │  filters  │
    │ filters.delaunay            │ Perform Delaunay triangulation of a pointcloud                                  │  filters  │
    │ filters.dem                 │ Filter points about an elevation surface                                        │  filters  │
    │ filters.divider             │ Divide points into approximately equal sized groups based on a simple scheme    │  filters  │
    │ filters.eigenvalues         │ Returns the eigenvalues for a given point, based on its k-nearest neighbors.    │  filters  │
    │ filters.elm                 │ Marks low points as noise.                                                      │  filters  │
    │ filters.estimaterank        │ Computes the rank of a neighborhood of points.                                  │  filters  │
    │ filters.expression          │ Pass only points given an expression                                            │  filters  │
    │ filters.expressionstats     │ Accumulate count statistics for a given dimension for an array of expressions   │  filters  │
    │ filters.faceraster          │ Face Raster Filter                                                              │  filters  │
    │      ·                      │      ·                                                                          │     ·     │
    │      ·                      │      ·                                                                          │     ·     │
    │      ·                      │      ·                                                                          │     ·     │
    │ readers.slpk                │ SLPK Reader                                                                     │  readers  │
    │ readers.smrmsg              │ SBET smrmsg Reader                                                              │  readers  │
    │ readers.stac                │ STAC Reader                                                                     │  readers  │
    │ readers.terrasolid          │ TerraSolid Reader                                                               │  readers  │
    │ readers.text                │ Text Reader                                                                     │  readers  │
    │ readers.tindex              │ TileIndex Reader                                                                │  readers  │
    │ writers.copc                │ COPC Writer                                                                     │  writers  │
    │ writers.ept_addon           │ EPT Writer                                                                      │  writers  │
    │ writers.fbi                 │ FBI Writer                                                                      │  writers  │
    │ writers.gdal                │ Write a point cloud as a GDAL raster.                                           │  writers  │
    │ writers.gltf                │ Gltf Writer                                                                     │  writers  │
    │ writers.las                 │ ASPRS LAS 1.0 - 1.4 writer                                                      │  writers  │
    │ writers.ogr                 │ Write a point cloud as a set of OGR points/multipoints                          │  writers  │
    │ writers.pcd                 │ Write data in the Point Cloud Library (PCL) format.                             │  writers  │
    │ writers.ply                 │ ply writer                                                                      │  writers  │
    │ writers.raster              │ Write a raster.                                                                 │  writers  │
    │ writers.sbet                │ SBET Writer                                                                     │  writers  │
    │ writers.text                │ Text Writer                                                                     │  writers  │
    ├─────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────┴───────────┤
    │ 119 rows (40 shown)                                                                                             3 columns │
    └───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
    ```

+ ### PDAL_Read

    You can read point cloud data from a file with:

    ```sql
    SELECT * FROM PDAL_Read('path/to/your/pointcloud.las');

    ┌───────────┬───────────┬────────┬───────┐
    │     X     │     Y     │   Z    │       │
    │   double  │   double  │ double │  ...  │
    ├───────────┼───────────┼────────┤───────┤
    │ 637177.98 │ 849393.95 │ 411.19 │  ...  │
    │ 637177.30 │ 849396.95 │ 411.25 │  ...  │
    │ 637176.34 │ 849400.84 │ 411.01 │  ...  │
    │ 637175.45 │ 849404.62 │ 410.99 │  ...  │
    │ 637174.33 │ 849407.37 │ 411.38 │  ...  │
    └───────────┴───────────┴────────┴───────┘
    ```

    Setting options is also possible. For example, to read a subset of points starting from index 10:

    ```sql
    SELECT
        X, Y, Z, Red, Green, Blue
    FROM
        PDAL_Read('./test/data/autzen_trim.laz', options => MAP {'start': 10})
    ;

    ┌───────────┬───────────┬─────────┬────────┬────────┬────────┐
    │     X     │    Y      │    Z    │  Red   │ Green  │  Blue  │
    │   double  │  double   │  double │ uint16 │ uint16 │ uint16 │
    ├───────────┼───────────┼─────────┼────────┼────────┼────────┤
    │ 637173.82 │ 849395.08 │   11.22 │     79 │     94 │     88 │
    │ 637171.38 │ 849403.41 │  411.15 │     78 │     95 │     90 │
    │ 637172.27 │ 849399.57 │  411.22 │     80 │     94 │     89 │
    │ 637173.87 │ 849392.61 │  411.12 │     77 │     93 │     86 │
    │ 637176.11 │ 849383.07 │  411.29 │     80 │    100 │     90 │
    │     ·     │     ·     │     ·   │      · │      · │      · │
    │     ·     │     ·     │     ·   │      · │      · │      · │
    │     ·     │     ·     │     ·   │      · │      · │      · │
    │ 637173.82 │ 849395.08 │  411.22 │     79 │     94 │     88 │
    │ 637173.82 │ 849395.08 │  411.22 │     79 │     94 │     88 │
    ├───────────┴───────────┴─────────┴────────┴────────┴────────┤
    │ 110000 rows (40 shown)                           6 columns │
    └────────────────────────────────────────────────────────────┘
    ```

    PDAL supports to load raster files, then:

    ```sql
    SELECT
        X, Y, band_1 AS Red, band_2 AS Green, band_3 AS Blue
    FROM
        PDAL_Read('./test/data/overlay-sample.tiff')
    LIMIT 3
    ;

    ┌──────────┬───────────┬───────┬───────┬───────┐
    │    X     │     Y     │  Red  │ Green │ Blue  │
    │  double  │  double   │ uint8 │ uint8 │ uint8 │
    ├──────────┼───────────┼───────┼───────┼───────┤
    │ 545540.0 │ 4724510.0 │    60 │    44 │    18 │
    │ 545540.5 │ 4724510.0 │    60 │    44 │    18 │
    │ 545541.0 │ 4724510.0 │     8 │     3 │     0 │
    └──────────┴───────────┴───────┴───────┴───────┘
    ```

+ ### PDAL_Info

    To get information about the point cloud files without reading all the data, use the `PDAL_Info` function:

    ```sql
    SELECT
        *
    FROM
        PDAL_Info('./test/data/autzen_trim.la*')
    ;

    ┌──────────────────────┬─────────────┬───────────┬───────────┬────────┬───┬──────────┬──────────┬──────────┐
    │      file_name       │ point_count │   min_x   │  min_y    │ min_z  │ … │ offset_x │ offset_y │ offset_z │
    │       varchar        │   uint64    │  double   │ double    │ double │   │  double  │  double  │  double  │
    ├──────────────────────┼─────────────┼───────────┼───────────┼────────┼───┼──────────┼──────────┼──────────┤
    │ ./test/data/autzen…  │      110000 │ 636001.76 │ 848935.20 │ 406.26 │ … │      0.0 │      0.0 │      0.0 │
    │ ./test/data/autzen…  │      110000 │ 636001.76 │ 848935.20 │ 406.26 │ … │      0.0 │      0.0 │      0.0 │
    ├──────────────────────┴─────────────┴───────────┴───────────┴────────┴───┴──────────┴──────────┴──────────┤
    │ 2 rows                                                                             32 columns (10 shown) │
    └──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
    ```

    Use the `dimensions` column to get the list of fields available in each point:

    ```sql
    SELECT
    	UNNEST(dimensions).name AS name,
	    UNNEST(dimensions).type AS type
    FROM
	    PDAL_Info('./test/data/autzen_trim.laz')
    ;

    ┌───────────────────┬──────────┐
    │       name        │   type   │
    │      varchar      │ varchar  │
    ├───────────────────┼──────────┤
    │ X                 │ double   │
    │ Y                 │ double   │
    │ Z                 │ double   │
    │ Intensity         │ uint16_t │
    │ ReturnNumber      │ uint8_t  │
    │ NumberOfReturns   │ uint8_t  │
    │ ScanDirectionFlag │ uint8_t  │
    │ EdgeOfFlightLine  │ uint8_t  │
    │ Classification    │ uint8_t  │
    │ . . .             │ . . .    │
    │ Red               │ uint16_t │
    │ Green             │ uint16_t │
    │ Blue              │ uint16_t │
    ├───────────────────┴──────────┤
    │ 20 rows            2 columns │
    └──────────────────────────────┘
    ```

+ ### PDAL_Pipeline

    The `PDAL_Pipeline` function runs a PDAL pipeline before getting the data, using a JSON file as parameter:

    ```sql
    SELECT
        COUNT(*)
    FROM
        PDAL_pipeline('./test/data/autzen_trim.las', './test/data/autzen-pipeline.json')
    ;

    ┌──────────────┐
    │ count_star() │
    │    int64     │
    ├──────────────┤
    │     100      │
    └──────────────┘
    ```

    The pipeline can be provided either as a JSON file or as an inline JSON string. If the second parameter value
    starts with "[" and ends with "]", it represents an inline JSON, otherwise it is a file path:

    ```sql
    SELECT
        COUNT(*)
    FROM
        PDAL_pipeline('./test/data/autzen_trim.las',
            '[
                {
                    "type": "filters.tail",
                    "count": 10
                }
            ]'
        )
    ;
    ┌──────────────┐
    │ count_star() │
    │    int64     │
    ├──────────────┤
    │     10       │
    └──────────────┘
    ```

    The pipeline can contain any valid PDAL pipeline definition. See the [PDAL documentation](https://pdal.io/en/stable/pipeline.html) for more details.

    For example, the following pipeline returns only the last 100 points:

    ```json
    {
        "pipeline": [
            {
                "type": "filters.tail",
                "count": 100
            }
        ]
    }
    ```

    A more complex example, load a raster file, and using `filters.overlay`, extract attributes from a Geopackage:

    ```sql
    WITH __input AS (
    	SELECT
            X, Y, RasterValue
        FROM
            PDAL_Pipeline('./test/data/overlay-sample.tiff', './test/data/overlay-sample-pipeline.json')
    )
    SELECT
        COUNT(*) AS c,
        SUM(RasterValue) AS s
    FROM
        __input
    ;

    ┌───────┬──────────┐
    │   c   │    s     │
    │ int64 │  double  │
    ├───────┼──────────┤
    │ 57600 │ 576000.0 │
    └───────┴──────────┘
    ```

    Where the pipeline is:

    ```json
    {
        "pipeline": [
            {
                "type": "filters.assign",
                "value" : [
                    "RasterValue = 0.0"
                ]
            },
            {
                "type": "filters.overlay",
                "datasource": "./test/data/overlay-sample.gpkg",
                "layer": "area",
                "column": "user_data",
                "dimension": "RasterValue"
            }
        ]
    }
    ```

    If you want to write your own pipeline, you can use the `PDAL_Drivers` function to get the list of supported readers, filters and writers.

+ ### COPY TO PDAL (aka PDAL_Write)

    This extension injects into the `COPY TO` statement the `PDAL` format. It allows to export data from DuckDB to an external point cloud file, in any of supported PDAL writers: https://pdal.io/en/stable/stages/writers.html

    `CREATION_OPTIONs` allows to customize the output, each driver type defines its own options, read them in its specific guide.

    ```sql
    COPY (
    	SELECT * AS aa FROM './test/data/autzen_trim.laz'
    )
    TO
	    './test/data/autzen_new.laz'
    WITH (
        FORMAT PDAL,
        DRIVER 'LAS',
        CREATION_OPTIONS (
            'COMPRESSION=true',
            'MINOR_VERSION=4',
            'DATAFORMAT_ID=7',
            'OFFSET_X=0.0',
            'OFFSET_Y=0.0',
            'OFFSET_Z=0.0',
            'SCALE_X=auto',
            'SCALE_Y=auto',
            'SCALE_Z=auto'
        )
    );
    ```

    All input attributes with types not supported by PDAL are ignored. In addition, each `writer` type only supports a specific set of `dimensions`,
    so more input attributes could be ignored in the output.

### Supported Functions and Documentation

The full list of functions and their documentation is available in the [function reference](docs/functions.md)

## How do I build it?

### Dependencies

You need a recent version of CMake (3.5) and a C++14 compatible compiler.

We also highly recommend that you install [Ninja](https://ninja-build.org) which you can select when building by setting the `GEN=ninja` environment variable.
```
git clone --recurse-submodules https://github.com/ahuarte47/duckdb-pdal
cd duckdb-pdal
make release
```

You can then invoke the built DuckDB (with the extension statically linked)
```
./build/release/duckdb
```

Please see the Makefile for more options, or the extension template documentation for more details.

### Running the tests

Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:

```sh
make test
```

### Installing the deployed binaries

To install your extension binaries from S3, you will need to do two things. Firstly, DuckDB should be launched with the
`allow_unsigned_extensions` option set to true. How to set this will depend on the client you're using. Some examples:

CLI:
```shell
duckdb -unsigned
```

Python:
```python
con = duckdb.connect(':memory:', config={'allow_unsigned_extensions' : 'true'})
```

NodeJS:
```js
db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

Secondly, you will need to set the repository endpoint in DuckDB to the HTTP url of your bucket + version of the extension
you want to install. To do this run the following SQL query in DuckDB:
```sql
SET custom_extension_repository='bucket.s3.eu-west-1.amazonaws.com/<your_extension_name>/latest';
```
Note that the `/latest` path will allow you to install the latest extension version available for your current version of
DuckDB. To specify a specific version, you can pass the version instead.

After running these steps, you can install and load your extension using the regular INSTALL/LOAD commands in DuckDB:
```sql
INSTALL pdal;
LOAD pdal;
```

Enjoy!
