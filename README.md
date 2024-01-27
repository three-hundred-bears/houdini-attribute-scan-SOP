# Attribute Scan SOP

This project is my first attempt at both writing something practical in C++ and for the HDK. 
There are numerous improvements to be made, but in the interest of continuing my growth in 
both of these areas I've chosen to leave this project in its current state and move onto 
other projects rather than continue iterating on this one.

## Overview
This project contains the source code for a SOP node that allows a user to ensure that a 
connected geometry stream has a specified set of attributes, and that those attributes 
contain (or do not contain) specified values. 

For example, you're able to specify that incoming geometry should contain vertex attribute
`N`, `N` should be a **vector3**, and `N` should never equal `(0, 0, 0)`. If any of these 
requirements are not met, the Attribute Scan will throw an error detailing which conditions 
were missed. 

This node was intended to be used in a studio setting to ensure that inter-departmental 
handoffs contain all necessary attributes and are free of any possibly troublesome values. 
No one likes to get all the way to Lighting/Rendering before finding out their uvs are
missing or otherwise unusable.

## Examples
The below image will ensure that point attribute `v` and vertex attribute `uv` are present on
the geometry, and that `uv` is a **vector3** that cannot equal `(0, 0, 0)` on any vertex.

![example01](https://i.imgur.com/FVJVROG.jpeg)

This one ensures that point attribute `active` and primitive attribute `path` are present on the 
geometry, `path` is a **string** and does not contain "op:" anywhere in value, and that `active` is an 
**integer** attribute.

![example02](https://i.imgur.com/Z91mMEf.jpeg)

Lastly, here's an example of an error that would occur when failing to meet both requirements
of the second example.

![example03](https://i.imgur.com/utvBohY.jpeg)

___

Tested with Houdini 19.5.435 - Py 3.9, CMake 3.26.4, MSVC 14.29.30133
