(module lp-compose racket/gui
  
  (require ffi/unsafe)
  
  ;;----------------------------------------------------------------------------
  
  (define preferences-group%
    (class pane%
      (define fraction 0.5)
      
      (define/override (container-size info)
        (let ((lw (caar   info))
              (lh (cadar  info))
              (rw (caadr  info))
              (rh (cadadr info)))
            
            (values (* 2 (max lw rw)) (max lh rh))))
      
      (define/override (place-children info width height)
        (let ((lw (caar   info))
              (lh (cadar  info))
              (rw (caadr  info))
              (rh (cadadr info))
              (x  (quotient width 2)))
          
            (list (list (- x lw) 0 lw height)
                  (list    x     0 rw height))))
      
      (super-new (stretchable-height #f))))
  
  (define view%
    (class canvas%
      (inherit refresh with-gl-context swap-gl-buffers)
      
;     (define lib (ffi-lib "/Users/rlk/Projects/lightprobe/lp-render"))
      (define lib (ffi-lib "/home/rlk/Projects/lightprobe/lp-render"))

      (define draw (get-ffi-obj "draw" lib (_fun _int _int -> _void)))
  
      (define/override (on-paint)
        (with-gl-context
         (lambda ()
           (let ((w (send this get-width))
                 (h (send this get-height)))
             (draw w h)
             (swap-gl-buffers)))))
      
      (super-new (style '(gl hscroll vscroll)))))
  
  ;;----------------------------------------------------------------------------

  (define mode-options%
    (class preferences-group%
      (super-new)
      
      (define label (new message%
                         [parent this]
                         [label "Mode:"]))
      (define radio (new radio-box%
                         [parent this]
                         [label #f]
                         [choices '("Image" "Environment")]
                         [style   '(vertical)]))
      ))
  
  (define view-options%
    (class preferences-group%
      (super-new)
      
      (define label (new message%
                         [parent this]
                         [label "View:"]))
      (define group (new vertical-pane%
                         [parent this]
                         [alignment '(left top)]))
      (define grid  (new check-box%
                         [parent group]
                         [label "Show Grid"]))
      (define tone  (new check-box%
                         [parent group]
                         [label "Tone-Map"]))
      ))

  (define image-list%
    (class vertical-pane%
      (super-new)
      
      (define images  (new list-box%
                           [parent this]
                           [label #f]
                           [choices '("foo" "bar" "baz")]
                           [vert-margin 12]
                           [horiz-margin 8]
                           [style '(vertical-label extended)]))
      (define buttons (new horizontal-pane%
                           [parent this]
                           [stretchable-height #f]))
      (define add     (new button%
                           [parent buttons]
                           [label "Add"]))
      (define rem     (new button%
                           [parent buttons]
                           [label "Remove"]))
      ))

  (define root
    (let* ((frame (new frame%
                       (label "Lightprobe Composer")
                       (style '(metal))))
           
           ; Lay out the window with options on the left and view on the right.
           
           (columns (new horizontal-pane% (parent frame)   (min-height 200)))
           (left    (new vertical-pane%   (parent columns) (min-width  200)))
           (right   (new vertical-pane%   (parent columns) (min-width  200)))
           
           ; Create the options
           
           (imgs-group (new group-box-panel%
                            [parent left]
                            [label "Images"]))
           (opts-group (new group-box-panel%
                            [parent left]
                            [label "Options"]
                            [stretchable-height #f]))
           
           (images     (new image-list%      [parent imgs-group]))
           
           (mode-opts  (new mode-options%    [parent opts-group]))
           (view-opts  (new view-options%    [parent opts-group]))
           
           ; Create the view canvas and view controls.
           
           (view  (new view%
                       (parent right)
                       (min-width  640)
                       (min-height 480)))
           
           (sliders (new horizontal-pane%
                         (parent right)
                         (stretchable-height #f)))
           
           (exposure (new slider%
                      (parent sliders)
                      (label "Exposure")
                      (min-value   1)
                      (max-value 100)
                      (style '(horizontal plain))))
           
           (zoom (new slider%
                      (parent sliders)
                      (label "Zoom")
                      (min-value   1)
                      (max-value 100)
                      (style '(horizontal plain))))
           
           )
      frame))
  
  (send root show #t))