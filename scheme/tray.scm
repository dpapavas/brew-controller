(import (scheme inexact) (srfi 1)
        (gamma inexact) (gamma transformation) (gamma polygons)
        (gamma polyhedra) (gamma volumes) (gamma selection)
        (gamma operations) (gamma write))

(define-option draft? #false)

(define remesh-target (if draft? 1/2 1/4))
(define remesh-iterations (if draft? 1 2))

(set-curve-tolerance! (if draft? 1/15 1/50))

(define size 90)
(define height 20)
(define bevel 4)
(define fillet 130)
(define width 3)
(define chamfer 1)

(define-output tray
  (let* ((l (- size bevel bevel))
         (h (- height bevel chamfer 1/10))

         (xy (λ (p x) (rotate (p 0 -1 0 x) 45 2)))
         (z (λ (p x) (p 0 0 -1 (- height x))))
         (part (λ (x)
                 (~> (rectangle l l)
                     (union _ (~> (regular-polygon 3 l)
                                  (scale _ 3/4 1)
                                  (intersection _ (flush-south (rectangle (* 1 l) (* 4/5 l))))
                                  (rotate _ 45)))
                     (offset _ fillet)
                     (difference (rectangle 1000 1000) _)
                     (minkowski-sum _ (circle fillet))
                     (with-curve-tolerance 1/1000 _)
                     (difference (rectangle 1000 1000) _)

                     (linear-extrusion _ x 1/10)
                     (minkowski-sum _ (apply
                                       hull
                                       (list-for ((s (list 0 -1)))
                                         (translate
                                          (clip (sphere (- bevel x)) (plane 0 0 1 0))
                                          0 0 (* s h)))))))))

    (~> (difference
         (part 0)
         (let* ((w (- width chamfer chamfer)))
           (clip
            (part w)
            (rotate
             (plane 0 0 -1 (* -1 (+ (* 4/5 l) (- bevel w)) (cos-degrees 45)))
             45 1 1 0))))

        (flush-bottom _)

        (minkowski-sum _ (clip (sphere 1) (plane 0 0 -1 0)))
        (corefine _ (z plane 4))

        (remesh
         _
         (faces-in (z bounding-halfspace 4))
         (edges-in (z bounding-plane chamfer))
         remesh-target remesh-iterations)

        (deform
          _
          (vertices-in (z bounding-halfspace 3))
          (vertices-in (z bounding-halfspace chamfer))
          (transformation-append
           (apply scaling (make-list 3 94/100))
           (translation 0 0 3)) 0)

        (fair _ (vertices-in
                 (intersection
                  (z bounding-halfspace 3)
                  (rotate (z bounding-halfspace chamfer) 180 0))) 1)

        (deform
          _
          (vertices-in
           (intersection
            (z bounding-halfspace 3)
            (xy bounding-halfspace 0)))

          (vertices-in
           (intersection
            (z bounding-halfspace -1)
            (xy bounding-halfspace 57)))
          (transformation-append
           (apply scaling (make-list 3 11/10))
           (translation -11 11 (+ height 47))
           (rotation -60 1 1 0))

          0))))
