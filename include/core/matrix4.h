#pragma once

#include <array>
#include <ostream>

#include "core/real.h"
#include "core/vector3.h"

namespace eng
{

/**
 * Class represents a 4x4 matrix.
 *
 * Elements are stored in row-major order.
 */
class Matrix4
{
    public:

        /**
         * Constructs a new identity Matrix4.
         */
        Matrix4();

        /**
         * Constructs a new matrix with the supplied (row-major) values.
         *
         * @param elements
         *   Row major elements.
         */
        explicit Matrix4(const std::array<real, 16> &elements);

        /**
         * Static method to create an orthographic projection matrix.
         *
         * @param width
         *   width of window.
         *
         * @param height
         *   height of window.
         *
         * @param depth
         *   Depth of rendering view.
         *
         * @returns
         *   An orthographic projection matrix.
         */
        static Matrix4 make_projection(
            const real width,
            const real height,
            const real depth);

        /**
         * Make a Matrix4 that can be used as a view matrix for a camera.
         *
         * @param eye
         *   Position of the camera.
         *
         * @param look_at
         *   The point where the camera is looking.
         *
         * @param up
         *   The up vector of the camera.
         *
         * @returns
         *   A Matrix4 that can be used as a camera view matrix.
         */
        static Matrix4 make_look_at(
            const Vector3 &eye,
            const Vector3 &look_at,
            const Vector3 &up);

        /**
         * Static method to create a scale matrix.
         *
         * @param scale
         *   Vector3 specifying amount to scale along each axis.
         *
         * @returns
         *   Scale transformation matrix.
         */
        static Matrix4 make_scale(const Vector3 &scale);

        /**
         * Static method to create translation matrix.
         *
         * @param translate
         *   Vector to translate by.
         */
        static Matrix4 make_translate(const Vector3 &translate);

        /**
         * Static method to create a rotation matrix about the z axis.
         *
         * @param angle
         *   Angle to rotate by in radians.
         */
        static Matrix4 make_rotate_z(const real angle);

        /**
         * Performs matrix multiplication.
         *
         * @param matrix
         *   The Matrix4 to multiply.
         *
         * @returns
         *   This Matrix4 multiplied the supplied Matrix4.
         */
        Matrix4& operator*=(const Matrix4 &matrix);

        /**
         * Performs Matrix4 multiplication.
         *
         * @param matrix
         *   The Matrix4 to multiply.
         *
         * @returns
         *   New Matrix4 which is this Matrix4 multiplied the supplied Matrix4.
         */
        Matrix4 operator*(const Matrix4 &matrix) const;

        /**
         * Multiply this matrix by a given vector3.
         *
         * Internally this extends the Vector3 to have a fourth element with
         * a value of 1.0
         *
         * @param vector
         *   Vector3 to multiply by.
         *
         * @returns
         *   This matrix multiplied by the supplied vector3.
         */
        Vector3 operator*(const Vector3 &vector) const;

        /**
         * Get a reference to the element at the supplied index.
         *
         * @param index
         *   Index of element to get.
         *
         * @returns
         *   Reference to element at supplied index.
         */
        real& operator[](const size_t index);

        /**
         * Get a copy of the element at the supplied index.
         *
         * @param index
         *   Index of element to get.
         *
         * @returns
         *   Copy of element at supplied index.
         */
        real operator[](const size_t index) const;

        /**
         * Equality operator.
         *
         * @param other
         *   Matrix4 to check for equality.
         *
         * @returns
         *   True if both VMatrix4 objects are the same, false otherwise.
         */
        bool operator==(const Matrix4 &other) const;

        /**
         * Inequality operator.
         *
         * @param other
         *   Matrix4 to check for inequality.
         *
         * @returns
         *   True if both Matrix4 objects are not the same, false otherwise.
         */
        bool operator!=(const Matrix4 &other) const;

        /**
         * Get a pointer to the start of the internal Matrix4 data array.
         *
         * @returns
         *   Pointer to start if Matrix4 data.
         */
        const real* data() const;

        /**
         * Get a column from the matrix and return as a vector3. This ignores
         * the bottom row of the matrix.
         *
         * @param index
         *   The index of the column to return.
         *
         * @returns
         *   The first three value of the supplied column.
         */
        Vector3 column(const std::size_t index) const;

        /**
         * Writes the Matrix4 to the stream, useful for debugging.
         *
         * @param out
         *   The stream to write to.
         *
         * @param m
         *   The Matrix4 to write to the stream.
         *
         * @return
         *   A reference to the supplied stream, after the Matrix4 has been
         *   written.
         */
        friend std::ostream& operator<<( std::ostream &out, const Matrix4 &m);

    private:

        /** Matrix4 data */
        std::array<real, 16u> elements_;
};

}

