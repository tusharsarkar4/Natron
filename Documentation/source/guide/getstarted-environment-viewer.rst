.. for help on writing/extending this file, see the reStructuredText cheatsheet
   http://github.com/ralsina/rst-cheatsheet/raw/master/rst-cheatsheet.pdf
   
Compositing viewers
===================

Playback frame range
--------------------

The frame-range selector in the Viewer player controls determines the range
used by playback, the first/last frame buttons, and frame wrapping:

* **Global** uses the project frame range and follows changes made in Project
  Settings.
* **Input** uses the union of the active A and B Viewer input ranges. If no
  input is connected, it falls back to the project range.
* **In/Out** uses the custom playback in and out points. Editing either point,
  or dragging a playback boundary in the timeline, selects this mode
  automatically.
* **Visible** uses the integer frames currently visible in the timeline and
  follows timeline zooming, panning, and resizing.

The selected source and the custom In/Out range are stored with the project.

.. toctree::
   :maxdepth: 2
