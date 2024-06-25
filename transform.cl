__kernel void transform(__global float *draw, __global float *point, __global float *transform) {
    // get the index of the current element to be processed
    int i = get_global_id(0);

    // only use start of rows in the array
    i *= 4;
    
    // vectorMult() - calculates the product of the point vector and transform matrix, 
    // stores the result in the draw vector
    int col, element;
    for (col=0; col<4; col++) {
        draw[i+col] = 0.0; 
        for (element=0; element<4; element++) {
            draw[i+col] += point[i+element] * transform[0+(element*4)+col]; 
        }
    }

    // scale the points for curses screen resolution
    draw[i+0] *= 20;
    draw[i+0] += 50;
        
    draw[i+1] *= 20;
    draw[i+1] += 50;

    draw[i+2] *= 20;
    draw[i+2] += 50;
}