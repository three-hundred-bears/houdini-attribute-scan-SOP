<div align="center">

# `Attribute Scan SOP`

The `Attribute Scan SOP` provides a simple method of validating the existence, typing, and values present on the given attributes of the input geometry. 
This allows for quick and easy "pre-flight checks" on any outgoing geometry prior to an export or handoff.

</div>

## Introduction
The `Attribute Scan SOP` is a geometry operator created with the intent of allowing a user to easily validate the attributes on the connected geometry stream, ensuring that they conform to the provided parameters.

This includes:
- ensuring an attribute does (or does not) exist on a given class of geometry
- ensuring an attribute is of a specific data type
- ensuring an attribute does (or does not) contain a given value

For example, you're able to specify that incoming geometry should contain vertex attribute
`N`, `N` should be a **vector3**, and `N` should never equal `(0, 0, 0)`. If any of these 
requirements are not met, the Attribute Scan will throw an error detailing which conditions 
were missed. 

> [!NOTE]
> This node is primarily intended to be used in a studio setting to ensure that inter-departmental handoffs contain all necessary attributes and are free of any erroneous values. No one likes getting all the way to lighting/rendering before finding out that half their uvs are missing.

## Quickstart

### Example: Validating UVs and Velocity 

Let's say we'd like to ensure that a point velocity attribute `v` and a vertex UV attribute `uv` are both present on our geometry. Additionally, we also want to make sure that *all* our geometry has proper UVs.

1. Add `v` and `uv` to the Point and Vertex attribute lists, respectively.

    This tells the node ensure their presence on the geometry.
2. Create additional scan parameters for `uv` by adding a row to the `Recognized Attribs` multiparm.

3. Set the `Scope` to "Vertex".

4. Set the `Type` to "vector3".

5. Enable `Ensure Value`, leave the value at (0, 0, 0), and switch the method from "Present" to "Absent".

    This will ensure that the given attribute value is completely absent from our geometry.

Altogether, this will force the node to validate that point attribute `v` and vertex attribute `uv` are present on the geometry, and that `uv` is a **vector3** that cannot equal `(0, 0, 0)` on any vertex.

![example01](https://i.imgur.com/FVJVROG.jpeg)

### Example: Validating Primitive Path and Active Geometry

Let's say we're exporting our geometry as an Alembic or USD, and want to ensure that it has a sufficiently descriptive path attribute (or at the very least, isn't default). We also want to include our `active` point attribute.

1. Add `active` and `path` to the Point and Primitive attribute lists, respectively.
2. Create additional scan parameters for `active` and `path` by adding two new rows to the `Recognized Attribs` multiparm.
3. Update the `Scope` and `Type` parameters to reflect the desired scan parameters for both attributes.
4. Enable `Ensure Value` underneath the `path` multiparm row, set the value to `op:`, and switch the method from "Present" to "Absent"

    This will ensure that the `op:` substring is completely absent from our path attributes.


Altogether, this will force the node to validate that point attribute `active` and primitive attribute `path` are present on the geometry, that `active` is an **integer**, and that `path` is a **string** that cannot contain the `op:` substring.

![example02](https://i.imgur.com/Z91mMEf.jpeg)

### Example: Failed Validation

Assuming that we fail to meet any of the requirements set in the prior example, the `Attribute Scan SOP` will throw a detailed error telling you what has gone wrong.

Here, we can see that our `active` attribute is not an int, and that our `path` attribute has an undesired value, meaning at least one of our primitives includes `op:` in its path.

![example03](https://i.imgur.com/utvBohY.jpeg)

## Installation

Please see my `HDK - How To` guide for step by step instructions on how to build Houdini plugins for Windows (Linux guide coming soon).

___

Tested with Houdini 19.5.435 - Py 3.9 / 20.0.668 - Py 3.10, CMake 3.26.4, MSVC 14.29.30133
