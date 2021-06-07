Micro BGP Suite history
=======================

This document summarizes a bit of history of the project, it tells you anything
you didn't want to know about `ubgpsuite` and never cared to ask.

`ubgpsuite` was created as an evolution over `bgpscanner` and its companion
library, `isocore`. `bgpscanner` was originally developed by me starting in 2017
as part of the Isolario Project, under the Institute of Informatics and
Telematics of the Italian National Research Council
[IIT-CNR](https://www.iit.cnr.it/).
`bgpscanner` was covered by the MIT license terms and is still available
(as of May 2021) at Isolario's
[website](https://isolario.it/web_content/php/site_content/tools.php).
Despite the fact that `bgpscanner` code was developed mostly by me, the
help of the Isolario team, their patience, and their knowledge about BGP's
nuts and bolts was invaluable to get it rolling.

By mid 2019, my collaboration with IIT-CNR has ceased.
In an attempt to further the concepts behind `bgpscanner` and keep experimenting
with them, I started the `ubgpsuite` project. At this time I was involved with
[Alpha Cogs](https://www.alphacogs.com), a company I co-founded, so I undertook
`ubgpsuite` development and got it moving forward with its financial support.

`ubgpsuite` was born by a partial rewrite of `bgpscanner`, taking advantage of
the fact that there was no more interoperability constrain with a larger
project -- `isocore` was originally intended to be used by other
components inside the Isolario project as well, but that wasn't the
case anymore. This allowed some minor improvement to the codebase, but the
overall software architecture was unchanged. Though `ubgpsuite` was relicensed
under the terms of LGPLv3+ for library code and GPLv3+ for utilities and tools.
The choice was motivated by my intention to keep the project free
(as in freedom) forever, considering that the only reason I was able to continue
developing the code I authored at IIT-CNR and keep the project alive was
its original open source license.

Unfortunately a chronical lack of time, due to more pressing company priorities,
has hit the fan during that year and most of 2020. But finally,
by the end of 2020, I got the chance to dedicate some time to the project.
As a matter of fact, I left Alpha Cogs and devoted a bit of myself to
move `ubgpsuite` and other projects I cared about out of stagnation.
`ubgpsuite` was then outside Alpha Cogs and became the first of
DoubleFourteen Code Forge projects -- 1414° for short, an initiative I
founded to research and develop free software, in the hope of improving
the software ecosystem.

On 2021, a total rewrite of `ubgpsuite` was complete and ready for release,
with this document included. History of the project will thereafter remain
unread.

Keep developing the code you love and enjoy,

---

Lorenzo Cogotti
