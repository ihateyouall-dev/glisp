;;; math.lisp - Numerical and mathematical utilities

(defun 1+ (x)
  (+ x 1))

(defun 1- (x)
  (- x 1))

(defun abs (x)
  (if (< x 0)
      (- 0 x)
      x))

(defun min (a b)
  (if (< a b)
      a
      b))

(defun max (a b)
  (if (> a b)
      a
      b))

(defun clamp (x low high)
  (cond
    ((< x low)  low)
    ((> x high) high)
    (else       x)))

(defun pow (x power)
  (cond
    ((= power 1) x)
    (else (* x (pow x (1- power))))))
