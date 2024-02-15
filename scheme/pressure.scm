;; -*- mode:scheme; coding: utf-8; -*-

;; Copyright 2024 Dimitris Papavasiliou

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU Affero General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU Affero General Public License for more details.

;; You should have received a copy of the GNU Affero General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

(import (srfi 1) (srfi 166) (scheme inexact)
        (gamma inexact)
        (gamma transformation) (gamma polygons) (gamma polyhedra)
        (gamma operations) (gamma write))

(define-option draft? #false)
(set-curve-tolerance! (if draft? 1/15 1/50))

(define chamfer-length 3/4)
(define wall-width 3)
(define tab-width 18)
(define tab-length 28)

(define apothem 22/2)

(define chamfer-kernel
  (let ((c (* 2 chamfer-length)))
    (rotate (square-pyramid c c chamfer-length) -90 0)))

(define (radius α) (/ α (cos-degrees 30)))

(define (sensor-body δ)
  (union
   (rotate (prism 6 (radius (+ apothem δ)) (+ 9 δ δ)) 30 2)
   (~> (cylinder (+ 21/2 δ) (+ 33/2 δ))
       (flush-top _)
       (translate _ 0 0 -9/2))))

(define-output sensor
  (union
   (sensor-body 0)
   (~> (cylinder 20/2 2)
       (flush-bottom _)
       (translate _ 0 0 9/2))
   (~> (cylinder 19/4 11)
       (flush-bottom _)
       (translate _ 0 0 13/2))))

(define-output sensor-mount
  (let* ((ε 1/2)
         (δ (+ wall-width ε (- chamfer-length)))
         (α (+ apothem δ))
         (r (radius α))
         (part (λ (x) (sensor-body x))))

    (~> (part δ)

        (union
         _
         (~> (cuboid r α (+ 9/2 33/2 δ))
             (flush _ 0 -1 1)
             (translate _ 0 (- α) 0)))

        (difference
           _
           (~> (cuboid r (* 2 3/2) (+ 3 (* 2 chamfer-length)))
               (flush _ 0 0 -1)
               (translate _ 0 (- α) -14))
           (flush-bottom (cylinder (+ 20/2 ε chamfer-length) tab-length))
           (flush-top (cylinder (+ 11/2 ε chamfer-length) tab-length)))

        (clip _ (plane 0 1 0 (- chamfer-length)))
        (minkowski-sum _ chamfer-kernel)
        (difference
         _
         (part ε)
         (let ((r (* 3/2 11/10)))
            (~> (circle r)
                (extrusion
                 _
                 (translation 0 0 -50)
                 (translation 0 0 0)
                 (transformation-append
                  (translation 0 0 r)
                  (scaling 2 2 1))
                 (transformation-append
                  (translation 0 0 (+ r 50))
                  (scaling 2 2 1)))
                (rotate _ -90 0)
                (translate _ 0 (+ (* wall-width 3/5) chamfer-length (- α) -9/5) -18)))))))

(define-output assembly
  (union sensor sensor-mount))
